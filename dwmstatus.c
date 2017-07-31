#include <X11/Xlib.h>

#if defined(DEBUG) && DEBUG > 0
#define DEBUG_PRINT(fmt, ...) fprintf(stderr, "[DEBUG] %s:%d:%s(): " fmt, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define DEBUG_ERROR(fmt) fprintf(stderr, "[ERROR] %s:%d:%s(): " fmt, __FILE__, __LINE__, __func__)
#else
#define DEBUG_PRINT(fmt, ...)
#define DEBUG_ERROR(fmt)
#endif

int main() {
	Display* display;
	display = XOpenDisplay(NULL);
	if (display == NULL) {
		return 1;
	}

	XCloseDisplay(display);

	return 0;
}
