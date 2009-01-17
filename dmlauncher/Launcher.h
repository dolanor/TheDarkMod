#ifndef _LAUNCHER_H_
#define _LAUNCHER_H_

#include <string>
#include "boost/filesystem.hpp"

namespace fs = boost::filesystem;

/**
 * greebo: Service class with platform-specific implementation
 * to launch the TDM game using the correct arguments.
 */
class Launcher
{
	// The path to the darkmod game base
	fs::path _darkmodDir;

	// The name of the current FM
	std::string _currentFM;

	// The name of the engine (Win32: DOOM3.exe)
	std::string _executable;

	// The arguments to pass to the engine
	std::string _arguments;

public:
	Launcher(const fs::path& darkmodDir);

	// Launches the game, reading the settings and arguments from the DarkmodDir
	void Launch();

private:
	// Reads the saved command line arguments
	void InitArguments();

	// Loads the name of the current FM
	void InitCurrentFM();

	// Reads the given text file and returns its contents
	std::string ReadFile(const fs::path& fileName);
};

#endif /* _LAUNCHER_H_ */