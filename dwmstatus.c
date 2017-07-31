#include <stdio.h>
#include <unistd.h>
#include <time.h>

#include <X11/Xlib.h>

#if defined(DEBUG) && DEBUG > 0
#define DEBUG_PRINT(fmt, ...) fprintf(stderr, "[DEBUG] %s:%d:%s(): " fmt, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define DEBUG_ERROR(fmt) fprintf(stderr, "[ERROR] %s:%d:%s(): " fmt, __FILE__, __LINE__, __func__)
#else
#define DEBUG_PRINT(fmt, ...)
#define DEBUG_ERROR(fmt)
#endif

int GetTime(char* out, size_t size) {
	time_t t;
	struct tm* localTime;

	time(&t);
	localTime = localtime(&t);
	if (localTime == NULL) {
		DEBUG_ERROR("Failed to get localTime\n");
		return 60;
	}

	int len = strftime(out, size, "%W %a %d %b %H:%M:%S", localTime);
	if (len <= 0) {
		DEBUG_ERROR("Failed to format time\n");
	}
	return localTime->tm_sec;
}

void SetStatus(char* status, Display* display) {
	XStoreName(display, DefaultRootWindow(display), status);
	XSync(display, False);
}

int main() {
	Display* display;
	display = XOpenDisplay(NULL);
	if (display == NULL) {
		DEBUG_ERROR("Failed to open display");
		return 1;
	}

	const size_t timeMaxLength = 32;
	const size_t statusMaxLength = 128;
	char time[timeMaxLength];
	char status[statusMaxLength];

	int sleepDuration = 0;
	while (True) {

		sleepDuration = 60 - GetTime(time, timeMaxLength);

		snprintf(status, statusMaxLength, "W%s", time);
		SetStatus(status, display);
		DEBUG_PRINT("Set status, sleeping for %d seconds\n", sleepDuration);
		sleep(sleepDuration);
	}

	XCloseDisplay(display);

	return 0;
}
