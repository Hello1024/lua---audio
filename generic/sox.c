#ifndef TH_GENERIC_FILE
#define TH_GENERIC_FILE "generic/sox.c"
#else

/* ---------------------------------------------------------------------- */
/* -- */
/* -- Copyright (c) 2012 Soumith Chintala */
/* --  */
/* -- Permission is hereby granted, free of charge, to any person obtaining */
/* -- a copy of this software and associated documentation files (the */
/* -- "Software"), to deal in the Software without restriction, including */
/* -- without limitation the rights to use, copy, modify, merge, publish, */
/* -- distribute, sublicense, and/or sell copies of the Software, and to */
/* -- permit persons to whom the Software is furnished to do so, subject to */
/* -- the following conditions: */
/* --  */
/* -- The above copyright notice and this permission notice shall be */
/* -- included in all copies or substantial portions of the Software. */
/* --  */
/* -- THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, */
/* -- EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF */
/* -- MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND */
/* -- NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE */
/* -- LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION */
/* -- OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION */
/* -- WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */
/* --  */
/* ---------------------------------------------------------------------- */
/* -- description: */
/* --     sox.c - a wrapper from libSox to Torch-7 */
/* -- */
/* -- history:  */
/* --     May 24th, 2012, 8:38PM - wrote load function - Soumith Chintala */
/* ---------------------------------------------------------------------- */

static sox_signalinfo_t siginfo;

static THTensor * libsox_(read_audio_file)(const char *file_name)
{
  // Create sox objects and read into int32_t buffer
  sox_format_t *fd;
  fd = sox_open_read(file_name, NULL, NULL, NULL);
  if (fd == NULL)
    abort_("[read_audio_file] Failure to read file");
  
  // Save signalinfo.
  memcpy(&siginfo, &fd->signal, sizeof(sox_signalinfo_t));
  
  int nchannels = fd->signal.channels;
  long buffer_size = fd->signal.length;
  int32_t *buffer = (int32_t *)malloc(sizeof(int32_t) * buffer_size);
  size_t samples_read = sox_read(fd, buffer, buffer_size);
  if (samples_read == 0)
    abort_("[read_audio_file] Empty file or read failed in sox_read");
  // alloc tensor 
  THTensor *tensor = THTensor_(newWithSize2d)(nchannels, samples_read / nchannels );
  tensor = THTensor_(newContiguous)(tensor);
  real *tensor_data = THTensor_(data)(tensor);
  // convert audio to dest tensor 
  int x,k;
  for (k=0; k<nchannels; k++) {
    for (x=0; x<samples_read/nchannels; x++) {
      *tensor_data++ = (real)buffer[x*nchannels+k];
    }
  }
  // free buffer and sox structures
  sox_close(fd);
  free(buffer);
  THTensor_(free)(tensor);

  // return tensor 
  return tensor;
}

static void libsox_(save_audio_file)(THTensor *input, const char *file_name)
{
  if (!siginfo.channels)
    abort_("[save_audio_file] You need to load a file first (to set sample rate etc. parameters).");
  
  if (THTensor_(size)(input, 1) != siginfo.channels || THTensor_(nDimension)(input) != 2)
    abort_("[save_audio_file] Tensor should be n x channels");

  sox_format_t * out = sox_open_write(file_name, &siginfo, NULL, NULL, NULL, NULL);
  
  if (!out)
    abort_("[save_audio_file] Open file for write failled.");
  
  THTensor * tensor = THTensor_(newContiguous)(input);
  real *tensor_data = THTensor_(data)(tensor);
  int buffer_size = THTensor_(size)(input, 0) * siginfo.channels;
  int32_t *buffer = (int32_t *)malloc(sizeof(int32_t) * buffer_size);
  int k;
  for (k=0; k<buffer_size; k++) {
    buffer[k] = (int32_t)tensor_data[k];
  }
  
  size_t written = sox_write(out, buffer, THTensor_(size)(input, 0));
  if (written != THTensor_(size)(input, 0))
    abort_("[save_audio_file] Partial write.");

  // free buffer and sox structures
  sox_close(out);
  free(buffer);
  THTensor_(free)(tensor);
}

static int libsox_(Main_load)(lua_State *L) {
  const char *filename = luaL_checkstring(L, 1);
  THTensor *tensor = libsox_(read_audio_file)(filename);
  luaT_pushudata(L, tensor, torch_Tensor);
  return 1;
}

static int libsox_(Main_save)(lua_State *L) {
  THTensor *input = luaT_checkudata(L, 1, torch_Tensor);
  const char *filename = luaL_checkstring(L, 2);
  libsox_(save_audio_file)(input, filename);
  return 0;
}

static const luaL_Reg libsox_(Main__)[] =
{
  {"load", libsox_(Main_load)},
  {"save", libsox_(Main_save)},
  {NULL, NULL}
};

DLL_EXPORT int libsox_(Main_init)(lua_State *L)
{
  luaT_pushmetatable(L, torch_Tensor);
  luaT_registeratname(L, libsox_(Main__), "libsox");
  // Initialize sox library
  sox_format_init();
  return 1;
}

#endif
