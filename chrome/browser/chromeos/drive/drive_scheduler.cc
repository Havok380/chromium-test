// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/drive_scheduler.h"

#include <math.h>

#include "base/message_loop.h"
#include "base/prefs/pref_service.h"
#include "base/rand_util.h"
#include "base/stl_util.h"
#include "chrome/browser/chromeos/drive/drive_file_system_util.h"
#include "chrome/browser/google_apis/drive_api_parser.h"
#include "chrome/browser/google_apis/gdata_wapi_parser.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace drive {

namespace {
const int kMaxThrottleCount = 5;
}

const int DriveScheduler::kMaxJobCount[] = {
  5,  // METADATA_QUEUE
  1,  // FILE_QUEUE
};

DriveScheduler::JobInfo::JobInfo(JobType in_job_type)
    : job_type(in_job_type),
      job_id(-1),
      completed_bytes(0),
      total_bytes(0),
      state(STATE_NONE) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

DriveScheduler::QueueEntry::QueueEntry(JobType in_job_type)
    : job_info(in_job_type),
      context(DriveClientContext(USER_INITIATED)) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

DriveScheduler::QueueEntry::~QueueEntry() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

bool DriveScheduler::QueueEntry::Compare(
    const DriveScheduler::QueueEntry* left,
    const DriveScheduler::QueueEntry* right) {
  return (left->context.type < right->context.type);
}

DriveScheduler::DriveScheduler(
    Profile* profile,
    google_apis::DriveServiceInterface* drive_service)
    : next_job_id_(0),
      throttle_count_(0),
      disable_throttling_(false),
      drive_service_(drive_service),
      uploader_(new google_apis::DriveUploader(drive_service)),
      profile_(profile),
      initialized_(false),
      weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  for (int i = 0; i < NUM_QUEUES; ++i) {
    jobs_running_[i] = 0;
  }
}

DriveScheduler::~DriveScheduler() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(initialized_);
  for (int i = 0; i < NUM_QUEUES; ++i) {
    STLDeleteElements(&queue_[i]);
  }
  net::NetworkChangeNotifier::RemoveConnectionTypeObserver(this);
}

void DriveScheduler::Initialize() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Initialize() may be called more than once for the lifetime when the
  // file system is remounted.
  if (initialized_)
    return;

  net::NetworkChangeNotifier::AddConnectionTypeObserver(this);
  initialized_ = true;
}

void DriveScheduler::GetAccountMetadata(
    const google_apis::GetAccountMetadataCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  scoped_ptr<QueueEntry> new_job(new QueueEntry(TYPE_GET_ACCOUNT_METADATA));
  new_job->get_account_metadata_callback = callback;

  QueueJob(new_job.Pass());

  StartJobLoop(GetJobQueueType(TYPE_GET_ACCOUNT_METADATA));
}

void DriveScheduler::GetAboutResource(
    const google_apis::GetAboutResourceCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  scoped_ptr<QueueEntry> new_job(new QueueEntry(TYPE_GET_ABOUT_RESOURCE));
  new_job->get_about_resource_callback = callback;

  QueueJob(new_job.Pass());

  StartJobLoop(GetJobQueueType(TYPE_GET_ABOUT_RESOURCE));
}

void DriveScheduler::GetAppList(
    const google_apis::GetAppListCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  scoped_ptr<QueueEntry> new_job(new QueueEntry(TYPE_GET_APP_LIST));
  new_job->get_app_list_callback = callback;

  QueueJob(new_job.Pass());

  StartJobLoop(GetJobQueueType(TYPE_GET_APP_LIST));
}

void DriveScheduler::GetAllResourceList(
    const google_apis::GetResourceListCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  scoped_ptr<QueueEntry> new_job(new QueueEntry(TYPE_GET_ALL_RESOURCE_LIST));
  new_job->get_resource_list_callback = callback;

  QueueJob(new_job.Pass());

  StartJobLoop(GetJobQueueType(TYPE_GET_ALL_RESOURCE_LIST));
}

void DriveScheduler::GetResourceListInDirectory(
    const std::string& directory_resource_id,
    const google_apis::GetResourceListCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  scoped_ptr<QueueEntry> new_job(
      new QueueEntry(TYPE_GET_RESOURCE_LIST_IN_DIRECTORY));
  new_job->directory_resource_id = directory_resource_id;
  new_job->get_resource_list_callback = callback;

  QueueJob(new_job.Pass());

  StartJobLoop(GetJobQueueType(TYPE_GET_RESOURCE_LIST_IN_DIRECTORY));
}

void DriveScheduler::Search(
    const std::string& search_query,
    const google_apis::GetResourceListCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  scoped_ptr<QueueEntry> new_job(new QueueEntry(TYPE_SEARCH));
  new_job->search_query = search_query;
  new_job->get_resource_list_callback = callback;

  QueueJob(new_job.Pass());

  StartJobLoop(GetJobQueueType(TYPE_SEARCH));
}

void DriveScheduler::GetChangeList(
    int64 start_changestamp,
    const google_apis::GetResourceListCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  scoped_ptr<QueueEntry> new_job(new QueueEntry(TYPE_GET_CHANGE_LIST));
  new_job->start_changestamp = start_changestamp;
  new_job->get_resource_list_callback = callback;

  QueueJob(new_job.Pass());

  StartJobLoop(GetJobQueueType(TYPE_GET_CHANGE_LIST));
}

void DriveScheduler::ContinueGetResourceList(
    const GURL& feed_url,
    const google_apis::GetResourceListCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  scoped_ptr<QueueEntry> new_job(
      new QueueEntry(TYPE_CONTINUE_GET_RESOURCE_LIST));
  new_job->feed_url = feed_url;
  new_job->get_resource_list_callback = callback;

  QueueJob(new_job.Pass());

  StartJobLoop(GetJobQueueType(TYPE_CONTINUE_GET_RESOURCE_LIST));
}

void DriveScheduler::GetResourceEntry(
    const std::string& resource_id,
    const DriveClientContext& context,
    const google_apis::GetResourceEntryCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  scoped_ptr<QueueEntry> new_job(new QueueEntry(TYPE_GET_RESOURCE_ENTRY));
  new_job->resource_id = resource_id;
  new_job->context = context;
  new_job->get_resource_entry_callback = callback;

  QueueJob(new_job.Pass());

  StartJobLoop(GetJobQueueType(TYPE_GET_RESOURCE_ENTRY));
}

void DriveScheduler::DeleteResource(
    const std::string& resource_id,
    const google_apis::EntryActionCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  scoped_ptr<QueueEntry> new_job(new QueueEntry(TYPE_DELETE_RESOURCE));
  new_job->resource_id = resource_id;
  new_job->entry_action_callback = callback;

  QueueJob(new_job.Pass());

  StartJobLoop(GetJobQueueType(TYPE_DELETE_RESOURCE));
}


void DriveScheduler::CopyHostedDocument(
    const std::string& resource_id,
    const std::string& new_name,
    const google_apis::GetResourceEntryCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  scoped_ptr<QueueEntry> new_job(new QueueEntry(TYPE_COPY_HOSTED_DOCUMENT));
  new_job->resource_id = resource_id;
  new_job->new_name = new_name;
  new_job->get_resource_entry_callback = callback;

  QueueJob(new_job.Pass());

  StartJobLoop(GetJobQueueType(TYPE_COPY_HOSTED_DOCUMENT));
}

void DriveScheduler::RenameResource(
    const std::string& resource_id,
    const std::string& new_name,
    const google_apis::EntryActionCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  scoped_ptr<QueueEntry> new_job(new QueueEntry(TYPE_RENAME_RESOURCE));
  new_job->resource_id = resource_id;
  new_job->new_name = new_name;
  new_job->entry_action_callback = callback;

  QueueJob(new_job.Pass());

  StartJobLoop(GetJobQueueType(TYPE_RENAME_RESOURCE));
}

void DriveScheduler::AddResourceToDirectory(
    const std::string& parent_resource_id,
    const std::string& resource_id,
    const google_apis::EntryActionCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  scoped_ptr<QueueEntry> new_job(
      new QueueEntry(TYPE_ADD_RESOURCE_TO_DIRECTORY));
  new_job->parent_resource_id = parent_resource_id;
  new_job->resource_id = resource_id;
  new_job->entry_action_callback = callback;

  QueueJob(new_job.Pass());

  StartJobLoop(GetJobQueueType(TYPE_ADD_RESOURCE_TO_DIRECTORY));
}

void DriveScheduler::RemoveResourceFromDirectory(
    const std::string& parent_resource_id,
    const std::string& resource_id,
    const google_apis::EntryActionCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  scoped_ptr<QueueEntry> new_job(
      new QueueEntry(TYPE_REMOVE_RESOURCE_FROM_DIRECTORY));
  new_job->parent_resource_id = parent_resource_id;
  new_job->resource_id = resource_id;
  new_job->entry_action_callback = callback;

  QueueJob(new_job.Pass());

  StartJobLoop(GetJobQueueType(TYPE_REMOVE_RESOURCE_FROM_DIRECTORY));
}

void DriveScheduler::AddNewDirectory(
    const std::string& parent_resource_id,
    const std::string& directory_name,
    const google_apis::GetResourceEntryCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  scoped_ptr<QueueEntry> new_job(new QueueEntry(TYPE_ADD_NEW_DIRECTORY));
  new_job->parent_resource_id = parent_resource_id;
  new_job->directory_name = directory_name;
  new_job->get_resource_entry_callback = callback;

  QueueJob(new_job.Pass());

  StartJobLoop(GetJobQueueType(TYPE_ADD_NEW_DIRECTORY));
}

void DriveScheduler::DownloadFile(
    const base::FilePath& virtual_path,
    const base::FilePath& local_cache_path,
    const GURL& download_url,
    const DriveClientContext& context,
    const google_apis::DownloadActionCallback& download_action_callback,
    const google_apis::GetContentCallback& get_content_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  scoped_ptr<QueueEntry> new_job(new QueueEntry(TYPE_DOWNLOAD_FILE));
  new_job->virtual_path = virtual_path;
  new_job->local_cache_path = local_cache_path;
  new_job->download_url = download_url;
  new_job->context = context;
  new_job->download_action_callback = download_action_callback;
  new_job->get_content_callback = get_content_callback;

  QueueJob(new_job.Pass());

  StartJobLoop(GetJobQueueType(TYPE_DOWNLOAD_FILE));
}

void DriveScheduler::UploadNewFile(
    const std::string& parent_resource_id,
    const base::FilePath& drive_file_path,
    const base::FilePath& local_file_path,
    const std::string& title,
    const std::string& content_type,
    const DriveClientContext& context,
    const google_apis::UploadCompletionCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  scoped_ptr<QueueEntry> new_job(new QueueEntry(TYPE_UPLOAD_NEW_FILE));
  new_job->resource_id = parent_resource_id;
  new_job->drive_file_path = drive_file_path;
  new_job->local_file_path = local_file_path;
  new_job->title = title;
  new_job->content_type = content_type;
  new_job->upload_completion_callback = callback;
  new_job->context = context;

  QueueJob(new_job.Pass());

  StartJobLoop(GetJobQueueType(TYPE_UPLOAD_NEW_FILE));
}

void DriveScheduler::UploadExistingFile(
    const std::string& resource_id,
    const base::FilePath& drive_file_path,
    const base::FilePath& local_file_path,
    const std::string& content_type,
    const std::string& etag,
    const DriveClientContext& context,
    const google_apis::UploadCompletionCallback& upload_completion_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  scoped_ptr<QueueEntry> new_job(new QueueEntry(TYPE_UPLOAD_EXISTING_FILE));
  new_job->resource_id = resource_id;
  new_job->drive_file_path = drive_file_path;
  new_job->local_file_path = local_file_path;
  new_job->content_type = content_type;
  new_job->etag = etag;
  new_job->upload_completion_callback = upload_completion_callback;
  new_job->context = context;

  QueueJob(new_job.Pass());

  StartJobLoop(GetJobQueueType(TYPE_UPLOAD_EXISTING_FILE));
}

void DriveScheduler::QueueJob(scoped_ptr<QueueEntry> job) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  QueueType queue_type = GetJobQueueType(job->job_info.job_type);
  std::list<QueueEntry*>& queue = queue_[queue_type];

  queue.push_back(job.release());
  queue.sort(&QueueEntry::Compare);
}

void DriveScheduler::StartJobLoop(QueueType queue_type) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (jobs_running_[queue_type] < kMaxJobCount[queue_type])
    DoJobLoop(queue_type);
}

void DriveScheduler::DoJobLoop(QueueType queue_type) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (queue_[queue_type].empty()) {
    return;
  }

  // Check if we should defer based on the first item in the queue
  if (ShouldStopJobLoop(queue_type, queue_[queue_type].front()->context)) {
    return;
  }

  // Increment the number of jobs.
  ++jobs_running_[queue_type];

  // Should copy before calling queue_.pop_front().
  scoped_ptr<QueueEntry> queue_entry(queue_[queue_type].front());
  queue_[queue_type].pop_front();

  JobInfo& job_info = queue_entry->job_info;
  job_info.state = STATE_RUNNING;

  // The some arguments are evaluated after bind, so we copy the pointer to the
  // QueueEntry
  QueueEntry* entry = queue_entry.get();

  switch (job_info.job_type) {
    case TYPE_GET_ABOUT_RESOURCE: {
      drive_service_->GetAboutResource(
          base::Bind(&DriveScheduler::OnGetAboutResourceJobDone,
                     weak_ptr_factory_.GetWeakPtr(),
                     base::Passed(&queue_entry)));
    }
    break;

    case TYPE_GET_ACCOUNT_METADATA: {
      drive_service_->GetAccountMetadata(
          base::Bind(&DriveScheduler::OnGetAccountMetadataJobDone,
                     weak_ptr_factory_.GetWeakPtr(),
                     base::Passed(&queue_entry)));
    }
    break;

    case TYPE_GET_APP_LIST: {
      drive_service_->GetAppList(
          base::Bind(&DriveScheduler::OnGetAppListJobDone,
                     weak_ptr_factory_.GetWeakPtr(),
                     base::Passed(&queue_entry)));
    }
    break;

    case TYPE_GET_ALL_RESOURCE_LIST: {
      drive_service_->GetAllResourceList(
          base::Bind(&DriveScheduler::OnGetResourceListJobDone,
                     weak_ptr_factory_.GetWeakPtr(),
                     base::Passed(&queue_entry)));
    }
    break;

    case TYPE_GET_RESOURCE_LIST_IN_DIRECTORY: {
      drive_service_->GetResourceListInDirectory(
          entry->directory_resource_id,
          base::Bind(&DriveScheduler::OnGetResourceListJobDone,
                     weak_ptr_factory_.GetWeakPtr(),
                     base::Passed(&queue_entry)));
    }
    break;

    case TYPE_SEARCH: {
      drive_service_->Search(
          entry->search_query,
          base::Bind(&DriveScheduler::OnGetResourceListJobDone,
                     weak_ptr_factory_.GetWeakPtr(),
                     base::Passed(&queue_entry)));
    }
    break;

    case TYPE_GET_CHANGE_LIST: {
      drive_service_->GetChangeList(
          entry->start_changestamp,
          base::Bind(&DriveScheduler::OnGetResourceListJobDone,
                     weak_ptr_factory_.GetWeakPtr(),
                     base::Passed(&queue_entry)));
    }
    break;

    case TYPE_CONTINUE_GET_RESOURCE_LIST: {
      drive_service_->ContinueGetResourceList(
          entry->feed_url,
          base::Bind(&DriveScheduler::OnGetResourceListJobDone,
                     weak_ptr_factory_.GetWeakPtr(),
                     base::Passed(&queue_entry)));
    }
    break;

    case TYPE_GET_RESOURCE_ENTRY: {
      drive_service_->GetResourceEntry(
          entry->resource_id,
          base::Bind(&DriveScheduler::OnGetResourceEntryJobDone,
                     weak_ptr_factory_.GetWeakPtr(),
                     base::Passed(&queue_entry)));
    }
    break;

    case TYPE_DELETE_RESOURCE: {
      drive_service_->DeleteResource(
          entry->resource_id,
          "",  // etag
          base::Bind(&DriveScheduler::OnEntryActionJobDone,
                     weak_ptr_factory_.GetWeakPtr(),
                     base::Passed(&queue_entry)));
    }
    break;


    case TYPE_COPY_HOSTED_DOCUMENT: {
      drive_service_->CopyHostedDocument(
          entry->resource_id,
          entry->new_name,
          base::Bind(&DriveScheduler::OnGetResourceEntryJobDone,
                     weak_ptr_factory_.GetWeakPtr(),
                     base::Passed(&queue_entry)));
    }
    break;

    case TYPE_RENAME_RESOURCE: {
      drive_service_->RenameResource(
          entry->resource_id,
          entry->new_name,
          base::Bind(&DriveScheduler::OnEntryActionJobDone,
                     weak_ptr_factory_.GetWeakPtr(),
                     base::Passed(&queue_entry)));
    }
    break;

    case TYPE_ADD_RESOURCE_TO_DIRECTORY: {
      drive_service_->AddResourceToDirectory(
          entry->parent_resource_id,
          entry->resource_id,
          base::Bind(&DriveScheduler::OnEntryActionJobDone,
                     weak_ptr_factory_.GetWeakPtr(),
                     base::Passed(&queue_entry)));
    }
    break;

    case TYPE_REMOVE_RESOURCE_FROM_DIRECTORY: {
      drive_service_->RemoveResourceFromDirectory(
          entry->parent_resource_id,
          entry->resource_id,
          base::Bind(&DriveScheduler::OnEntryActionJobDone,
                     weak_ptr_factory_.GetWeakPtr(),
                     base::Passed(&queue_entry)));
    }
    break;

    case TYPE_ADD_NEW_DIRECTORY: {
      drive_service_->AddNewDirectory(
          entry->parent_resource_id,
          entry->directory_name,
          base::Bind(&DriveScheduler::OnGetResourceEntryJobDone,
                     weak_ptr_factory_.GetWeakPtr(),
                     base::Passed(&queue_entry)));
    }
    break;

    case TYPE_DOWNLOAD_FILE: {
      drive_service_->DownloadFile(
          entry->virtual_path,
          entry->local_cache_path,
          entry->download_url,
          base::Bind(&DriveScheduler::OnDownloadActionJobDone,
                     weak_ptr_factory_.GetWeakPtr(),
                     base::Passed(&queue_entry)),
          entry->get_content_callback,
          google_apis::ProgressCallback());
    }
    break;

    case TYPE_UPLOAD_NEW_FILE: {
      uploader_->UploadNewFile(
          entry->resource_id,
          entry->drive_file_path,
          entry->local_file_path,
          entry->title,
          entry->content_type,
          base::Bind(&DriveScheduler::OnUploadCompletionJobDone,
                     weak_ptr_factory_.GetWeakPtr(),
                     base::Passed(&queue_entry)),
          google_apis::ProgressCallback());
    }
    break;

    case TYPE_UPLOAD_EXISTING_FILE: {
      uploader_->UploadExistingFile(
          entry->resource_id,
          entry->drive_file_path,
          entry->local_file_path,
          entry->content_type,
          entry->etag,
          base::Bind(&DriveScheduler::OnUploadCompletionJobDone,
                     weak_ptr_factory_.GetWeakPtr(),
                     base::Passed(&queue_entry)),
          google_apis::ProgressCallback());
    }
    break;

    // There is no default case so that there will be a compiler error if a type
    // is added but unhandled.
  }
}

bool DriveScheduler::ShouldStopJobLoop(QueueType queue_type,
                                       const DriveClientContext& context) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Should stop if the gdata feature was disabled while running the fetch
  // loop.
  if (profile_->GetPrefs()->GetBoolean(prefs::kDisableDrive))
    return true;

  // Should stop if the network is not online.
  if (net::NetworkChangeNotifier::IsOffline())
    return true;

  // Should stop background jobs if the current connection is on cellular
  // network, and fetching is disabled over cellular.
  bool should_stop_on_cellular_network = false;
  switch (context.type) {
    case USER_INITIATED:
      should_stop_on_cellular_network = false;
      break;
    case BACKGROUND:
    case PREFETCH:
      should_stop_on_cellular_network = (queue_type == FILE_QUEUE);
      break;
  }
  if (should_stop_on_cellular_network &&
      profile_->GetPrefs()->GetBoolean(prefs::kDisableDriveOverCellular) &&
      net::NetworkChangeNotifier::IsConnectionCellular(
          net::NetworkChangeNotifier::GetConnectionType())) {
    return true;
  }

  return false;
}

void DriveScheduler::ThrottleAndContinueJobLoop(QueueType queue_type) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (throttle_count_ < kMaxThrottleCount)
    throttle_count_++;

  base::TimeDelta delay;
  if (disable_throttling_) {
    delay = base::TimeDelta::FromSeconds(0);
  } else {
    delay =
      base::TimeDelta::FromSeconds(pow(2, throttle_count_ - 1)) +
      base::TimeDelta::FromMilliseconds(base::RandInt(0, 1000));
  }
  VLOG(1) << "Throttling for " << delay.InMillisecondsF();

  const bool posted = base::MessageLoopProxy::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&DriveScheduler::DoJobLoop,
                 weak_ptr_factory_.GetWeakPtr(),
                 queue_type),
      delay);
  DCHECK(posted);
}

void DriveScheduler::ResetThrottleAndContinueJobLoop(QueueType queue_type) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Post a task to continue the job loop.  This allows us to finish handling
  // the current job before starting the next one.
  throttle_count_ = 0;
  base::MessageLoopProxy::current()->PostTask(FROM_HERE,
      base::Bind(&DriveScheduler::DoJobLoop,
                 weak_ptr_factory_.GetWeakPtr(),
                 queue_type));
}

scoped_ptr<DriveScheduler::QueueEntry> DriveScheduler::OnJobDone(
    scoped_ptr<DriveScheduler::QueueEntry> queue_entry,
    DriveFileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  QueueType queue_type = GetJobQueueType(queue_entry->job_info.job_type);

  // Decrement the number of jobs for this queue.
  --jobs_running_[queue_type];

  // Retry, depending on the error.
  if (error == DRIVE_FILE_ERROR_THROTTLED) {
    queue_entry->job_info.state = STATE_RETRY;

    // Requeue the job.
    QueueJob(queue_entry.Pass());

    ThrottleAndContinueJobLoop(queue_type);

    return scoped_ptr<DriveScheduler::QueueEntry>();
  } else {
    ResetThrottleAndContinueJobLoop(queue_type);

    // Send the entry back.
    return queue_entry.Pass();
  }
}

void DriveScheduler::OnGetResourceListJobDone(
    scoped_ptr<DriveScheduler::QueueEntry> queue_entry,
    google_apis::GDataErrorCode error,
    scoped_ptr<google_apis::ResourceList> resource_list) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  DriveFileError drive_error(util::GDataToDriveFileError(error));

  scoped_ptr<QueueEntry> job_info = OnJobDone(queue_entry.Pass(), drive_error);

  if (!job_info)
    return;

  // Handle the callback.
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(job_info->get_resource_list_callback,
                 error,
                 base::Passed(&resource_list)));
}

void DriveScheduler::OnGetResourceEntryJobDone(
    scoped_ptr<DriveScheduler::QueueEntry> queue_entry,
    google_apis::GDataErrorCode error,
    scoped_ptr<google_apis::ResourceEntry> entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  DriveFileError drive_error(util::GDataToDriveFileError(error));

  scoped_ptr<QueueEntry> job_info = OnJobDone(queue_entry.Pass(), drive_error);

  if (!job_info)
    return;

  // Handle the callback.
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(job_info->get_resource_entry_callback,
                 error,
                 base::Passed(&entry)));
}

void DriveScheduler::OnGetAboutResourceJobDone(
    scoped_ptr<QueueEntry> queue_entry,
    google_apis::GDataErrorCode error,
    scoped_ptr<google_apis::AboutResource> about_resource) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  DriveFileError drive_error(util::GDataToDriveFileError(error));

  scoped_ptr<QueueEntry> job_info = OnJobDone(queue_entry.Pass(), drive_error);

  if (!job_info)
    return;

  // Handle the callback.
  job_info->get_about_resource_callback.Run(error, about_resource.Pass());
}

void DriveScheduler::OnGetAccountMetadataJobDone(
    scoped_ptr<QueueEntry> queue_entry,
    google_apis::GDataErrorCode error,
    scoped_ptr<google_apis::AccountMetadata> account_metadata) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  DriveFileError drive_error(util::GDataToDriveFileError(error));

  scoped_ptr<QueueEntry> job_info = OnJobDone(queue_entry.Pass(), drive_error);

  if (!job_info)
    return;

  // Handle the callback.
  job_info->get_account_metadata_callback.Run(error, account_metadata.Pass());
}

void DriveScheduler::OnGetAppListJobDone(
    scoped_ptr<DriveScheduler::QueueEntry> queue_entry,
    google_apis::GDataErrorCode error,
    scoped_ptr<google_apis::AppList> app_list) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  DriveFileError drive_error(util::GDataToDriveFileError(error));

  scoped_ptr<QueueEntry> job_info = OnJobDone(queue_entry.Pass(), drive_error);

  if (!job_info)
    return;

  // Handle the callback.
  job_info->get_app_list_callback.Run(error, app_list.Pass());
}

void DriveScheduler::OnEntryActionJobDone(
    scoped_ptr<DriveScheduler::QueueEntry> queue_entry,
    google_apis::GDataErrorCode error) {
  DriveFileError drive_error(util::GDataToDriveFileError(error));

  scoped_ptr<QueueEntry> job_info = OnJobDone(queue_entry.Pass(), drive_error);

  if (!job_info)
    return;

  // Handle the callback.
  DCHECK(!job_info->entry_action_callback.is_null());
  job_info->entry_action_callback.Run(error);
}

void DriveScheduler::OnDownloadActionJobDone(
    scoped_ptr<DriveScheduler::QueueEntry> queue_entry,
    google_apis::GDataErrorCode error,
    const base::FilePath& temp_file) {
  DriveFileError drive_error(util::GDataToDriveFileError(error));

  scoped_ptr<QueueEntry> job_info = OnJobDone(queue_entry.Pass(), drive_error);

  if (!job_info)
    return;

  // Handle the callback.
  DCHECK(!job_info->download_action_callback.is_null());
  job_info->download_action_callback.Run(error, temp_file);
}

void DriveScheduler::OnUploadCompletionJobDone(
    scoped_ptr<QueueEntry> queue_entry,
    google_apis::DriveUploadError error,
    const base::FilePath& drive_path,
    const base::FilePath& file_path,
    scoped_ptr<google_apis::ResourceEntry> resource_entry) {
  DriveFileError drive_error(DriveUploadErrorToDriveFileError(error));

  scoped_ptr<QueueEntry> job_info = OnJobDone(queue_entry.Pass(), drive_error);

  if (!job_info)
    return;

  // Handle the callback.
  DCHECK(!job_info->upload_completion_callback.is_null());
  job_info->upload_completion_callback.Run(
      error, drive_path, file_path, resource_entry.Pass());
}

void DriveScheduler::OnConnectionTypeChanged(
    net::NetworkChangeNotifier::ConnectionType type) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Resume the job loop if the network is back online. Note that we don't
  // need to check the type of the network as it will be checked in
  // ShouldStopJobLoop() as soon as the loop is resumed.
  if (!net::NetworkChangeNotifier::IsOffline()) {
    for (int i = METADATA_QUEUE; i < NUM_QUEUES; ++i) {
      StartJobLoop(static_cast<QueueType>(i));
    }
  }
}

DriveScheduler::QueueType DriveScheduler::GetJobQueueType(JobType type) {
  switch (type) {
    case TYPE_GET_ABOUT_RESOURCE:
    case TYPE_GET_ACCOUNT_METADATA:
    case TYPE_GET_APP_LIST:
    case TYPE_GET_ALL_RESOURCE_LIST:
    case TYPE_GET_RESOURCE_LIST_IN_DIRECTORY:
    case TYPE_SEARCH:
    case TYPE_GET_CHANGE_LIST:
    case TYPE_CONTINUE_GET_RESOURCE_LIST:
    case TYPE_GET_RESOURCE_ENTRY:
    case TYPE_DELETE_RESOURCE:
    case TYPE_COPY_HOSTED_DOCUMENT:
    case TYPE_RENAME_RESOURCE:
    case TYPE_ADD_RESOURCE_TO_DIRECTORY:
    case TYPE_REMOVE_RESOURCE_FROM_DIRECTORY:
    case TYPE_ADD_NEW_DIRECTORY:
      return METADATA_QUEUE;

    case TYPE_DOWNLOAD_FILE:
    case TYPE_UPLOAD_NEW_FILE:
    case TYPE_UPLOAD_EXISTING_FILE:
      return FILE_QUEUE;
  }
  NOTREACHED();
  return FILE_QUEUE;
}

}  // namespace drive
