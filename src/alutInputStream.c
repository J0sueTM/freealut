#include "alutInternal.h"
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#if HAVE_STAT
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#define structStat struct stat
#elif HAVE__STAT
#define stat(p,b) _stat((p),(b))
#define structStat struct _stat
#else
#error No stat-like function on this platform
#endif

struct InputStream_struct
{
  ALboolean isFileStream;
  char *fileName;
  size_t remainingLength;
  union
  {
    FILE *fileDescriptor;       /* for file streams */
    const ALvoid *data;         /* for memory streams */
  } u;
};


/****************************************************************************
 * The functions below know the internal InputStream representation.
 ****************************************************************************/

InputStream *
_alutStreamFromFile (const char *fileName)
{
  InputStream *stream;
  structStat statBuf;
  FILE *fileDescriptor;
  char *fileNameBuffer;

  stream = (InputStream *) malloc (sizeof (InputStream));
  if (stream == NULL)
    {
      _alutSetError (ALUT_ERROR_OUT_OF_MEMORY);
      return NULL;
    }

  if (stat (fileName, &statBuf))
    {
      _alutSetError (ALUT_ERROR_IO_ERROR);
      free (stream);
      return NULL;
    }

  fileDescriptor = fopen (fileName, "rb");
  if (fileDescriptor == NULL)
    {
      _alutSetError (ALUT_ERROR_IO_ERROR);
      free (stream);
      return NULL;
    }

  fileNameBuffer = (char *) malloc (strlen (fileName) + 1);
  if (fileNameBuffer == NULL)
    {
      _alutSetError (ALUT_ERROR_OUT_OF_MEMORY);
      free (stream);
      return NULL;
    }

  stream->isFileStream = AL_TRUE;
  stream->fileName = strcpy (fileNameBuffer, fileName);
  stream->remainingLength = statBuf.st_size;
  stream->u.fileDescriptor = fileDescriptor;
  return stream;
}


InputStream *
_alutStreamFromMemory (const ALvoid *data, ALsizei length)
{
  InputStream *stream = (InputStream *) malloc (sizeof (InputStream));
  if (stream == NULL)
    {
      _alutSetError (ALUT_ERROR_OUT_OF_MEMORY);
      return NULL;
    }

  stream->isFileStream = AL_FALSE;
  stream->fileName = NULL;
  stream->remainingLength = length;
  stream->u.data = data;
  return stream;
}


ALboolean
_alutStreamDestroy (InputStream *stream)
{
  ALboolean status =
    (stream->isFileStream && fclose (stream->u.fileDescriptor)) ?
    AL_FALSE : AL_TRUE;
  if (stream->fileName)
    {
      free (stream->fileName);
    }
  free (stream);
  return status;
}


const char *
_alutStreamGetFileName (const InputStream *stream)
{
  return stream->fileName;
}

size_t
_alutStreamGetRemainingLength (const InputStream *stream)
{
  return stream->remainingLength;
}


ALboolean
_alutStreamEOF (InputStream *stream)
{
  if (stream->isFileStream)
    {
      int c = fgetc (stream->u.fileDescriptor);
      if (c != EOF)
        {
          ungetc (c, stream->u.fileDescriptor);
        }
      return (c == EOF) ? AL_TRUE : AL_FALSE;
    }
  else
    {
      return (stream->remainingLength == 0) ? AL_TRUE : AL_FALSE;
    }
}


ALboolean
_alutStreamRead (InputStream *stream, void *ptr, size_t numBytesToRead)
{
  if (stream->isFileStream)
    {
      size_t numBytesRead =
        fread (ptr, 1, numBytesToRead, stream->u.fileDescriptor);
      if (numBytesToRead != numBytesRead)
        {
          _alutSetError (ferror (stream->u.fileDescriptor) ?
                         ALUT_ERROR_IO_ERROR :
                         ALUT_ERROR_CORRUPT_OR_TRUNCATED_DATA);
          return AL_FALSE;
        }
      return AL_TRUE;
    }
  else
    {
      if (stream->remainingLength < numBytesToRead)
        {
          _alutSetError (ALUT_ERROR_CORRUPT_OR_TRUNCATED_DATA);
          return AL_FALSE;
        }
      memcpy (ptr, stream->u.data, numBytesToRead);
      stream->u.data = ((char *) (stream->u.data) + numBytesToRead);
      return AL_TRUE;
    }
}


/****************************************************************************
 * The utility functions below do not know the internal InputStream
 * representation.
 ****************************************************************************/

ALboolean
_alutStreamSkip (InputStream *stream, size_t numBytesToSkip)
{
  ALboolean status;
  char *buf;
  if (numBytesToSkip == 0)
    {
      return AL_TRUE;
    }
  buf = (char *) malloc (numBytesToSkip);
  if (buf == NULL)
    {
      _alutSetError (ALUT_ERROR_OUT_OF_MEMORY);
      return AL_FALSE;
    }
  status = _alutStreamRead (stream, buf, numBytesToSkip);
  free (buf);
  return status;
}

ALboolean
_alutReadUInt16LittleEndian (InputStream *stream, UInt16LittleEndian *value)
{
  unsigned char buf[2];
  if (!_alutStreamRead (stream, buf, sizeof (buf)))
    {
      return AL_FALSE;
    }
  *value = ((UInt16LittleEndian) buf[1] << 8) | ((UInt16LittleEndian) buf[0]);
  return AL_TRUE;
}

ALboolean
_alutReadInt32BigEndian (InputStream *stream, Int32BigEndian *value)
{
  unsigned char buf[4];
  if (!_alutStreamRead (stream, buf, sizeof (buf)))
    {
      return AL_FALSE;
    }
  *value =
    ((Int32BigEndian) buf[0] << 24) |
    ((Int32BigEndian) buf[1] << 16) |
    ((Int32BigEndian) buf[2] << 8) | ((Int32BigEndian) buf[3]);
  return AL_TRUE;
}

ALboolean
_alutReadUInt32LittleEndian (InputStream *stream, UInt32LittleEndian *value)
{
  unsigned char buf[4];
  if (!_alutStreamRead (stream, buf, sizeof (buf)))
    {
      return AL_FALSE;
    }
  *value =
    ((UInt32LittleEndian) buf[3] << 24) |
    ((UInt32LittleEndian) buf[2] << 16) |
    ((UInt32LittleEndian) buf[1] << 8) | ((UInt32LittleEndian) buf[0]);
  return AL_TRUE;
}
