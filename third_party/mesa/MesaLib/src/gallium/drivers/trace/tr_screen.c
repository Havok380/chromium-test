/**************************************************************************
 *
 * Copyright 2008 Tungsten Graphics, Inc., Cedar Park, Texas.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
 * IN NO EVENT SHALL TUNGSTEN GRAPHICS AND/OR ITS SUPPLIERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 **************************************************************************/

#include "util/u_format.h"
#include "util/u_memory.h"
#include "util/u_simple_list.h"

#include "tr_dump.h"
#include "tr_dump_state.h"
#include "tr_texture.h"
#include "tr_context.h"
#include "tr_screen.h"
#include "tr_public.h"

#include "pipe/p_format.h"


static boolean trace = FALSE;

static const char *
trace_screen_get_name(struct pipe_screen *_screen)
{
   struct trace_screen *tr_scr = trace_screen(_screen);
   struct pipe_screen *screen = tr_scr->screen;
   const char *result;

   trace_dump_call_begin("pipe_screen", "get_name");

   trace_dump_arg(ptr, screen);

   result = screen->get_name(screen);

   trace_dump_ret(string, result);

   trace_dump_call_end();

   return result;
}


static const char *
trace_screen_get_vendor(struct pipe_screen *_screen)
{
   struct trace_screen *tr_scr = trace_screen(_screen);
   struct pipe_screen *screen = tr_scr->screen;
   const char *result;

   trace_dump_call_begin("pipe_screen", "get_vendor");

   trace_dump_arg(ptr, screen);

   result = screen->get_vendor(screen);

   trace_dump_ret(string, result);

   trace_dump_call_end();

   return result;
}


static int
trace_screen_get_param(struct pipe_screen *_screen,
                       enum pipe_cap param)
{
   struct trace_screen *tr_scr = trace_screen(_screen);
   struct pipe_screen *screen = tr_scr->screen;
   int result;

   trace_dump_call_begin("pipe_screen", "get_param");

   trace_dump_arg(ptr, screen);
   trace_dump_arg(int, param);

   result = screen->get_param(screen, param);

   trace_dump_ret(int, result);

   trace_dump_call_end();

   return result;
}


static int
trace_screen_get_shader_param(struct pipe_screen *_screen, unsigned shader,
                       enum pipe_shader_cap param)
{
   struct trace_screen *tr_scr = trace_screen(_screen);
   struct pipe_screen *screen = tr_scr->screen;
   int result;

   trace_dump_call_begin("pipe_screen", "get_shader_param");

   trace_dump_arg(ptr, screen);
   trace_dump_arg(int, shader);
   trace_dump_arg(int, param);

   result = screen->get_shader_param(screen, shader, param);

   trace_dump_ret(int, result);

   trace_dump_call_end();

   return result;
}


static float
trace_screen_get_paramf(struct pipe_screen *_screen,
                        enum pipe_cap param)
{
   struct trace_screen *tr_scr = trace_screen(_screen);
   struct pipe_screen *screen = tr_scr->screen;
   float result;

   trace_dump_call_begin("pipe_screen", "get_paramf");

   trace_dump_arg(ptr, screen);
   trace_dump_arg(int, param);

   result = screen->get_paramf(screen, param);

   trace_dump_ret(float, result);

   trace_dump_call_end();

   return result;
}


static boolean
trace_screen_is_format_supported(struct pipe_screen *_screen,
                                 enum pipe_format format,
                                 enum pipe_texture_target target,
                                 unsigned sample_count,
                                 unsigned tex_usage,
                                 unsigned geom_flags)
{
   struct trace_screen *tr_scr = trace_screen(_screen);
   struct pipe_screen *screen = tr_scr->screen;
   boolean result;

   trace_dump_call_begin("pipe_screen", "is_format_supported");

   trace_dump_arg(ptr, screen);
   trace_dump_arg(format, format);
   trace_dump_arg(int, target);
   trace_dump_arg(uint, sample_count);
   trace_dump_arg(uint, tex_usage);
   trace_dump_arg(uint, geom_flags);

   result = screen->is_format_supported(screen, format, target, sample_count,
                                        tex_usage, geom_flags);

   trace_dump_ret(bool, result);

   trace_dump_call_end();

   return result;
}


static struct pipe_context *
trace_screen_context_create(struct pipe_screen *_screen, void *priv)
{
   struct trace_screen *tr_scr = trace_screen(_screen);
   struct pipe_screen *screen = tr_scr->screen;
   struct pipe_context *result;

   trace_dump_call_begin("pipe_screen", "context_create");

   trace_dump_arg(ptr, screen);

   result = screen->context_create(screen, priv);

   trace_dump_ret(ptr, result);

   trace_dump_call_end();

   result = trace_context_create(tr_scr, result);

   return result;
}


static void
trace_screen_flush_frontbuffer(struct pipe_screen *_screen,
                               struct pipe_surface *_surface,
                               void *context_private)
{
   struct trace_screen *tr_scr = trace_screen(_screen);
   struct trace_surface *tr_surf = trace_surface(_surface);
   struct pipe_screen *screen = tr_scr->screen;
   struct pipe_surface *surface = tr_surf->surface;

   trace_dump_call_begin("pipe_screen", "flush_frontbuffer");

   trace_dump_arg(ptr, screen);
   trace_dump_arg(ptr, surface);
   /* XXX: hide, as there is nothing we can do with this
   trace_dump_arg(ptr, context_private);
   */

   screen->flush_frontbuffer(screen, surface, context_private);

   trace_dump_call_end();
}


/********************************************************************
 * texture
 */


static struct pipe_resource *
trace_screen_resource_create(struct pipe_screen *_screen,
                            const struct pipe_resource *templat)
{
   struct trace_screen *tr_scr = trace_screen(_screen);
   struct pipe_screen *screen = tr_scr->screen;
   struct pipe_resource *result;

   trace_dump_call_begin("pipe_screen", "resource_create");

   trace_dump_arg(ptr, screen);
   trace_dump_arg(resource_template, templat);

   result = screen->resource_create(screen, templat);

   trace_dump_ret(ptr, result);

   trace_dump_call_end();

   result = trace_resource_create(tr_scr, result);

   return result;
}

static struct pipe_resource *
trace_screen_resource_from_handle(struct pipe_screen *_screen,
                                 const struct pipe_resource *templ,
                                 struct winsys_handle *handle)
{
   struct trace_screen *tr_screen = trace_screen(_screen);
   struct pipe_screen *screen = tr_screen->screen;
   struct pipe_resource *result;

   /* TODO trace call */

   result = screen->resource_from_handle(screen, templ, handle);

   result = trace_resource_create(trace_screen(_screen), result);

   return result;
}

static boolean
trace_screen_resource_get_handle(struct pipe_screen *_screen,
                                struct pipe_resource *_texture,
                                struct winsys_handle *handle)
{
   struct trace_screen *tr_screen = trace_screen(_screen);
   struct trace_resource *tr_texture = trace_resource(_texture);
   struct pipe_screen *screen = tr_screen->screen;
   struct pipe_resource *texture = tr_texture->resource;

   /* TODO trace call */

   return screen->resource_get_handle(screen, texture, handle);
}



static void
trace_screen_resource_destroy(struct pipe_screen *_screen,
			      struct pipe_resource *_texture)
{
   struct trace_screen *tr_scr = trace_screen(_screen);
   struct trace_resource *tr_tex = trace_resource(_texture);
   struct pipe_screen *screen = tr_scr->screen;
   struct pipe_resource *texture = tr_tex->resource;

   assert(texture->screen == screen);

   trace_dump_call_begin("pipe_screen", "texture_destroy");

   trace_dump_arg(ptr, screen);
   trace_dump_arg(ptr, texture);

   trace_dump_call_end();

   trace_resource_destroy(tr_scr, tr_tex);
}


/********************************************************************
 * surface
 */


static struct pipe_surface *
trace_screen_get_tex_surface(struct pipe_screen *_screen,
                             struct pipe_resource *_texture,
                             unsigned face, unsigned level,
                             unsigned zslice,
                             unsigned usage)
{
   struct trace_screen *tr_scr = trace_screen(_screen);
   struct trace_resource *tr_tex = trace_resource(_texture);
   struct pipe_screen *screen = tr_scr->screen;
   struct pipe_resource *texture = tr_tex->resource;
   struct pipe_surface *result = NULL;

   assert(texture->screen == screen);

   trace_dump_call_begin("pipe_screen", "get_tex_surface");

   trace_dump_arg(ptr, screen);
   trace_dump_arg(ptr, texture);
   trace_dump_arg(uint, face);
   trace_dump_arg(uint, level);
   trace_dump_arg(uint, zslice);
   trace_dump_arg(uint, usage);

   result = screen->get_tex_surface(screen, texture, face, level, zslice, usage);

   trace_dump_ret(ptr, result);

   trace_dump_call_end();

   result = trace_surface_create(tr_tex, result);

   return result;
}


static void
trace_screen_tex_surface_destroy(struct pipe_surface *_surface)
{
   struct trace_screen *tr_scr = trace_screen(_surface->texture->screen);
   struct trace_surface *tr_surf = trace_surface(_surface);
   struct pipe_screen *screen = tr_scr->screen;
   struct pipe_surface *surface = tr_surf->surface;

   trace_dump_call_begin("pipe_screen", "tex_surface_destroy");

   trace_dump_arg(ptr, screen);
   trace_dump_arg(ptr, surface);

   trace_dump_call_end();

   trace_surface_destroy(tr_surf);
}





/********************************************************************
 * buffer
 */



static struct pipe_resource *
trace_screen_user_buffer_create(struct pipe_screen *_screen,
                                void *data,
                                unsigned size,
				unsigned usage)
{
   struct trace_screen *tr_scr = trace_screen(_screen);
   struct pipe_screen *screen = tr_scr->screen;
   struct pipe_resource *result;

   trace_dump_call_begin("pipe_screen", "user_buffer_create");

   trace_dump_arg(ptr, screen);
   trace_dump_arg_begin("data");
   trace_dump_bytes(data, size);
   trace_dump_arg_end();
   trace_dump_arg(uint, size);
   trace_dump_arg(uint, usage);

   result = screen->user_buffer_create(screen, data, size, usage);

   trace_dump_ret(ptr, result);

   trace_dump_call_end();

   if(result) {
      assert(!(result->flags & TRACE_FLAG_USER_BUFFER));
      result->flags |= TRACE_FLAG_USER_BUFFER;
   }

   return trace_resource_create(tr_scr, result);
}




/********************************************************************
 * fence
 */


static void
trace_screen_fence_reference(struct pipe_screen *_screen,
                             struct pipe_fence_handle **pdst,
                             struct pipe_fence_handle *src)
{
   struct trace_screen *tr_scr = trace_screen(_screen);
   struct pipe_screen *screen = tr_scr->screen;
   struct pipe_fence_handle *dst;

   assert(pdst);
   dst = *pdst;
   
   trace_dump_call_begin("pipe_screen", "fence_reference");

   trace_dump_arg(ptr, screen);
   trace_dump_arg(ptr, dst);
   trace_dump_arg(ptr, src);

   screen->fence_reference(screen, pdst, src);

   trace_dump_call_end();
}


static int
trace_screen_fence_signalled(struct pipe_screen *_screen,
                             struct pipe_fence_handle *fence,
                             unsigned flags)
{
   struct trace_screen *tr_scr = trace_screen(_screen);
   struct pipe_screen *screen = tr_scr->screen;
   int result;

   trace_dump_call_begin("pipe_screen", "fence_signalled");

   trace_dump_arg(ptr, screen);
   trace_dump_arg(ptr, fence);
   trace_dump_arg(uint, flags);

   result = screen->fence_signalled(screen, fence, flags);

   trace_dump_ret(int, result);

   trace_dump_call_end();

   return result;
}


static int
trace_screen_fence_finish(struct pipe_screen *_screen,
                          struct pipe_fence_handle *fence,
                          unsigned flags)
{
   struct trace_screen *tr_scr = trace_screen(_screen);
   struct pipe_screen *screen = tr_scr->screen;
   int result;

   trace_dump_call_begin("pipe_screen", "fence_finish");

   trace_dump_arg(ptr, screen);
   trace_dump_arg(ptr, fence);
   trace_dump_arg(uint, flags);

   result = screen->fence_finish(screen, fence, flags);

   trace_dump_ret(int, result);

   trace_dump_call_end();

   return result;
}


/********************************************************************
 * screen
 */

static void
trace_screen_destroy(struct pipe_screen *_screen)
{
   struct trace_screen *tr_scr = trace_screen(_screen);
   struct pipe_screen *screen = tr_scr->screen;

   trace_dump_call_begin("pipe_screen", "destroy");
   trace_dump_arg(ptr, screen);
   trace_dump_call_end();
   trace_dump_trace_end();

   screen->destroy(screen);

   FREE(tr_scr);
}

boolean
trace_enabled(void)
{
   static boolean firstrun = TRUE;

   if (!firstrun)
      return trace;
   firstrun = FALSE;

   trace_dump_init();

   if(trace_dump_trace_begin()) {
      trace_dumping_start();
      trace = TRUE;
   }

   return trace;
}

struct pipe_screen *
trace_screen_create(struct pipe_screen *screen)
{
   struct trace_screen *tr_scr;
   struct pipe_winsys *winsys;

   if(!screen)
      goto error1;

   if (!trace_enabled())
      goto error1;

   trace_dump_call_begin("", "pipe_screen_create");

   tr_scr = CALLOC_STRUCT(trace_screen);
   if(!tr_scr)
      goto error2;

#if 0
   winsys = trace_winsys_create(screen->winsys);
   if(!winsys)
      goto error3;
#else
   winsys = screen->winsys;
#endif
   tr_scr->base.winsys = winsys;
   tr_scr->base.destroy = trace_screen_destroy;
   tr_scr->base.get_name = trace_screen_get_name;
   tr_scr->base.get_vendor = trace_screen_get_vendor;
   tr_scr->base.get_param = trace_screen_get_param;
   tr_scr->base.get_shader_param = trace_screen_get_shader_param;
   tr_scr->base.get_paramf = trace_screen_get_paramf;
   tr_scr->base.is_format_supported = trace_screen_is_format_supported;
   assert(screen->context_create);
   tr_scr->base.context_create = trace_screen_context_create;
   tr_scr->base.resource_create = trace_screen_resource_create;
   tr_scr->base.resource_from_handle = trace_screen_resource_from_handle;
   tr_scr->base.resource_get_handle = trace_screen_resource_get_handle;
   tr_scr->base.resource_destroy = trace_screen_resource_destroy;
   tr_scr->base.get_tex_surface = trace_screen_get_tex_surface;
   tr_scr->base.tex_surface_destroy = trace_screen_tex_surface_destroy;
   tr_scr->base.user_buffer_create = trace_screen_user_buffer_create;
   tr_scr->base.fence_reference = trace_screen_fence_reference;
   tr_scr->base.fence_signalled = trace_screen_fence_signalled;
   tr_scr->base.fence_finish = trace_screen_fence_finish;
   tr_scr->base.flush_frontbuffer = trace_screen_flush_frontbuffer;

   tr_scr->screen = screen;

   trace_dump_ret(ptr, screen);
   trace_dump_call_end();

   return &tr_scr->base;

error2:
   trace_dump_ret(ptr, screen);
   trace_dump_call_end();
   trace_dump_trace_end();
error1:
   return screen;
}


struct trace_screen *
trace_screen(struct pipe_screen *screen)
{
   assert(screen);
   assert(screen->destroy == trace_screen_destroy);
   return (struct trace_screen *)screen;
}
