// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_BROWSER_PHONE_NUMBER_I18N_H_
#define COMPONENTS_AUTOFILL_BROWSER_PHONE_NUMBER_I18N_H_

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/string16.h"

namespace i18n {
namespace phonenumbers {
class PhoneNumber;
}
}

// Utilities to process, normalize and compare international phone numbers.
namespace autofill_i18n {

// Most of the following functions require |region| to operate. The |region| is
// a ISO 3166 standard code ("US" for USA, "CZ" for Czech Republic, etc.).

// Parses the number stored in |value| as a phone number interpreted in the
// given |region|, and stores the results into the remaining arguments.  The
// |region| should be a 2-letter country code.  This is an internal function,
// exposed in the header file so that it can be tested.
bool ParsePhoneNumber(
    const base::string16& value,
    const std::string& region,
    base::string16* country_code,
    base::string16* city_code,
    base::string16* number,
    i18n::phonenumbers::PhoneNumber* i18n_number) WARN_UNUSED_RESULT;

// Normalizes phone number, by changing digits in the extended fonts
// (such as \xFF1x) into '0'-'9'. Also strips out non-digit characters.
base::string16 NormalizePhoneNumber(const base::string16& value,
                                    const std::string& region);

// Constructs whole phone number from parts.
// |city_code| - area code, could be empty.
// |country_code| - country code, could be empty.
// |number| - local number, should not be empty.
// |region| - current region, the parsing is based on.
// |whole_number| - constructed whole number.
// Separator characters are stripped before parsing the digits.
// Returns true if parsing was successful, false otherwise.
bool ConstructPhoneNumber(const base::string16& country_code,
                          const base::string16& city_code,
                          const base::string16& number,
                          const std::string& region,
                          base::string16* whole_number) WARN_UNUSED_RESULT;

// Returns true if |number_a| and |number_b| parse to the same phone number in
// the given |region|.
bool PhoneNumbersMatch(const base::string16& number_a,
                       const base::string16& number_b,
                       const std::string& region,
                       const std::string& app_locale);

// The cached phone number, does parsing only once, improves performance.
class PhoneObject {
 public:
  PhoneObject(const base::string16& number,
              const std::string& region);
  PhoneObject(const PhoneObject&);
  PhoneObject();
  ~PhoneObject();

  std::string region() const { return region_; }

  base::string16 country_code() const { return country_code_; }
  base::string16 city_code() const { return city_code_; }
  base::string16 number() const { return number_; }

  base::string16 GetFormattedNumber() const;
  base::string16 GetWholeNumber() const;

  PhoneObject& operator=(const PhoneObject& other);

  bool IsValidNumber() const { return i18n_number_ != NULL; }

 private:
  // The region code used to parse this number.
  std::string region_;

  // The parsed number and its components.
  scoped_ptr<i18n::phonenumbers::PhoneNumber> i18n_number_;
  base::string16 city_code_;
  base::string16 country_code_;
  base::string16 number_;

  // Pretty printed version of the whole number, or empty if parsing failed.
  // Set on first request.
  mutable base::string16 formatted_number_;

  // The whole number, normalized to contain only digits if possible.
  // Set on first request.
  mutable base::string16 whole_number_;
};

}  // namespace autofill_i18n

#endif  // COMPONENTS_AUTOFILL_BROWSER_PHONE_NUMBER_I18N_H_
