//
// main.cpp - Boot launcher
//
// Returning EXIT_REBOOT (instead of calling reboot()) lets Circle's sysinit
// start the chain boot of the selected kernel.
//
#include "kernel.h"
#include <circle/startup.h>

int main (void)
{
	// allocated on the heap and never destroyed: no destructors run before chain boot
	CKernel *pKernel = new CKernel;
	if (!pKernel->Initialize ())
	{
		halt ();
		return EXIT_HALT;
	}

	CStdlibApp::TShutdownMode ShutdownMode = pKernel->Run ();

	switch (ShutdownMode)
	{
	case CStdlibApp::ShutdownReboot:
		return EXIT_REBOOT;

	case CStdlibApp::ShutdownHalt:
	default:
		halt ();
		return EXIT_HALT;
	}
}
