#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "tests/test_env_cmake.h"

// Set to true after 10s, prevents race condition
static bool failFlag = false;

static FILE *pRead  = nullptr;
static FILE *pWrite = nullptr;

void signalHandle(int sig)
{
	printf("Child process received signal: %d\n", sig);

	if(sig == SIGTERM && !failFlag)
	{
		fwrite(TEST_PROC_STR_SIGTERM, 1, sizeof(TEST_PROC_STR_SIGTERM), pWrite);

		fclose(pRead);
		fclose(pWrite);

		printf("Stopping test child process\n");
		fflush(stdout);

		exit(0);
	}
}

int main(int argc, char *argv[])
{
	printf("Starting test child process\n");

	if(argc < 3)
		return -1;

	const int fdRead = atoi(argv[1]);
	const int fdWrite = atoi(argv[2]);

	pRead = fdopen(fdRead, "r");
	pWrite = fdopen(fdWrite, "w");

	signal(SIGTERM, signalHandle);

//	printf("Writing " TEST_PROC_STR_START "to pipe\n");

	fwrite(TEST_PROC_STR_START, 1, sizeof(TEST_PROC_STR_START), pWrite);
	fflush(pWrite);

//	printf("Finished writing " TEST_PROC_STR_START "to pipe\n");

	char readDat[50] = "";
	fread(readDat, 50, 1, pRead);

//	printf("Read %s from pipe\n", readDat);
	const char *pTestEnvVal = getenv(TEST_PROC_ENV_VAR_NAME);
	if(pTestEnvVal == nullptr)
		fwrite("", 1, sizeof(""), pWrite);
	else
		fwrite(pTestEnvVal, 1, strlen(pTestEnvVal)+1, pWrite);

	fflush(pWrite);

//	printf("Writing env var val \"%s\" to pipe\n", pTestEnvVal);
//	const char *const *pEnv = environ;
//	while(*pEnv != nullptr)
//	{
//		printf("%s\n", *pEnv);
//		pEnv += 1;
//	}

	sleep(10);

	// If SIGHUP was not received after 10 seconds, write fail and exit
	failFlag = true;
	fwrite(TEST_PROC_STR_FAIL, 1, sizeof(TEST_PROC_STR_FAIL), pWrite);

	fclose(pRead);
	fclose(pWrite);

	printf("Failed to stop test child process in time. Quitting...\n");

	return 0;
}
