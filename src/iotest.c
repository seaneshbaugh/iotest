#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

typedef struct timespec timespec;

#ifdef __APPLE__

#include <mach/mach_time.h>

timespec timeDiff(uint64_t start, uint64_t end) {
  static mach_timebase_info_data_t machTimebaseInfo = {0, 0};

  if (machTimebaseInfo.denom == 0) {
    mach_timebase_info(&machTimebaseInfo);
  }

  uint64_t difference = end - start;

  uint64_t elapsedNanoseconds = difference * (machTimebaseInfo.numer / machTimebaseInfo.denom);

  timespec result;

  result.tv_sec = elapsedNanoseconds * 1e-9;

  result.tv_nsec = elapsedNanoseconds - (result.tv_sec * 1e9);

  return result;
}

#elif __linux

timespec timeDiff(timespec start, timespec end) {
  timespec result;

  if (end.tv_nsec - start.tv_nsec < 0) {
    result.tv_sec = end.tv_sec - start.tv_sec - 1;

    result.tv_nsec = 1000000000 + end.tv_nsec - start.tv_nsec;
  } else {
    result.tv_sec = end.tv_sec - start.tv_sec;

    result.tv_nsec = end.tv_nsec - start.tv_nsec;
  }

  return result;
}

#else

#error Unknown and unsupported platform.

#endif

int main(int argc, char **argv) {
  char template[L_tmpnam];

  memset(template, 0, sizeof(template));

  strncpy(template, "/tmp/iotest-XXXXXX", 18);

  errno = 0;

  int fileDescriptor = mkstemp(template);

  if (fileDescriptor == -1) {
    fprintf(stderr, "Could not create temp file: %s\n", strerror(errno));

    exit(1);
  }

  errno = 0;

  FILE *file = fdopen(fileDescriptor, "wb+");

  if (!file) {
    fprintf(stderr, "Could not open temp file: %s\n", strerror(errno));

    unlink(template);

    exit(1);
  }

  // const size_t fileSize = 1073741824;

  const size_t fileSize = 134217728;

  printf("Writing %zu bytes to %s\n", fileSize, template);

#ifdef __APPLE__

  uint64_t startTime;

  startTime = mach_absolute_time();

#elif __linux

  timespec startTime;

  clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &startTime);

#endif

  errno = 0;

  for (size_t i = 0; i < fileSize; i += 1) {
    if (fwrite(&i, sizeof(char), 1, file) != 1) {
      fprintf(stderr, "Could not write to temp file: %s\n", strerror(errno));

      fclose(file);

      unlink(template);

      exit(1);
    }
  }

#ifdef __APPLE__

  uint64_t endTime;

  endTime = mach_absolute_time();

#elif __linux

  timespec endTime;

  clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &endTime);

#endif

  timespec elapsedTime = timeDiff(startTime, endTime);

  printf("%ld.%ld seconds elapsed\n", elapsedTime.tv_sec, elapsedTime.tv_nsec);

  printf("Beginning read tests\n");

  size_t fastestBuffer = 512;

  double fastestAverageTime = 0.0;

  for (size_t bufferSize = 512; bufferSize <= fileSize; bufferSize *= 2) {
    printf("\nTesting buffer size of %zu bytes\n", bufferSize);

    const size_t numberOfTrials = 16;

    timespec trials[numberOfTrials];

    for (size_t trial = 0; trial < numberOfTrials; trial += 1) {
      errno = 0;

      if (fseek(file, 0L, SEEK_SET) == -1) {
        fprintf(stderr, "Could not rewind temp file: %s\n", strerror(errno));

        fclose(file);

        unlink(template);

        exit(1);
      }

      errno = 0;

      char *buffer = (char*)calloc(bufferSize, sizeof(char));

      if (!buffer) {
        fprintf(stderr, "Could not allocate memory for read buffer: %s\n", strerror(errno));

        fclose(file);

        unlink(template);

        exit(1);
      }

#ifdef __APPLE__

      startTime = mach_absolute_time();

#elif __linux

      clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &startTime);

#endif

      size_t totalRead = 0;

      while (totalRead < fileSize && !feof(file)) {
        errno = 0;

        size_t itemsRead = fread(buffer, bufferSize, 1, file);

        if (itemsRead != 1) {
          fprintf(stderr, "Could not read from temp file: %s\n", strerror(errno));

          free(buffer);

          fclose(file);

          unlink(template);

          exit(1);
        }

        printf("\r%9zu bytes read", totalRead);

        totalRead += itemsRead * bufferSize;
      }

      printf("\r%9zu bytes read\n", totalRead);

#ifdef __APPLE__

      endTime = mach_absolute_time();

#elif __linux

      clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &endTime);

#endif

      trials[trial] = timeDiff(startTime, endTime);

      free(buffer);
    }

    printf("\n");

    double total = 0.0;

    for (size_t i = 0; i < numberOfTrials; i += 1) {
      printf("Trial %2zu: %ld.%09ld seconds\n", i, trials[i].tv_sec, trials[i].tv_nsec);

      total += (double)trials[i].tv_sec + ((double)trials[i].tv_nsec / 1000000000.0);
    }

    double average = total / 16.0;

    printf("Average: %f seconds\n", average);

    if (fastestAverageTime == 0.0 || average < fastestAverageTime) {
      fastestAverageTime = average;

      fastestBuffer = bufferSize;
    }
  }

  printf("\nFastest buffer size was %zu bytes with an average time of %f seconds (%f bytes per second)\n", fastestBuffer, fastestAverageTime, (double)fileSize / fastestAverageTime);

  fclose(file);

  unlink(template);

  return 0;
}
