#ifndef NEWSBOAT_TEST_HELPERS_H_
#define NEWSBOAT_TEST_HELPERS_H_

#include <cstdlib>
#include <cstring>
#include <exception>
#include <fstream>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

#include "3rd-party/catch.hpp"

namespace TestHelpers {

/* Objects of TempFile class generate a temporary filename and delete the
 * corresponding file when they are destructed.
 *
 * This is useful for teardown in tests, where we use RAII to clean up
 * temporary DB files and stuff. */
class TempFile {
public:
	class tempfileexception : public std::exception {
	public:
		explicit tempfileexception(const char* error)
		{
			msg = "failed to create a tempdir: ";
			msg += error;
		};

		virtual const char* what() const throw()
		{
			return msg.c_str();
		}

	private:
		std::string msg;
	};

	TempFile()
	{
		init_tempdir();
		init_file();
	}

	~TempFile()
	{
		::unlink(filepath.c_str());
		::rmdir(tempdir.c_str());
	}

	const std::string getPath()
	{
		return filepath;
	}

private:
	void init_tempdir()
	{
		char* tmpdir_p = ::getenv("TMPDIR");

		if (tmpdir_p) {
			tempdir = tmpdir_p;
		} else {
			tempdir = "/tmp/";
		}

		tempdir += "/newsboat-tests/";

		int status = mkdir(tempdir.c_str(), S_IRWXU);
		if (status != 0) {
			// The directory already exists. That's fine, though,
			// but only as long as it has all the properties we
			// need.

			int saved_errno = errno;
			bool success = false;

			if (saved_errno == EEXIST) {
				struct stat buffer;
				if (lstat(tempdir.c_str(), &buffer) == 0) {
					if (buffer.st_mode & S_IRUSR &&
						buffer.st_mode & S_IWUSR &&
						buffer.st_mode & S_IXUSR) {
						success = true;
					}
				}
			}

			if (!success) {
				throw tempfileexception(strerror(saved_errno));
			}
		}
	};

	void init_file()
	{
		bool success = false;
		unsigned int tries = 0;

		// Make 10 attempts at generating a filename that doesn't exist
		do {
			tries++;

			// This isn't thread-safe, but we don't care because
			// Catch doesn't let us run tests in multiple threads
			// anyway.
			std::string filename = std::to_string(rand());
			filepath = tempdir + "/" + filename;

			struct stat buffer;
			if (lstat(filepath.c_str(), &buffer) != 0) {
				if (errno == ENOENT) {
					success = true;
				}
			}
		} while (!success && tries < 10);

		if (!success) {
			throw tempfileexception(
				"failed to generate unique filename");
		}
	}

	std::string tempdir;
	std::string filepath;
};

/*
 * The AssertArticleFileContent opens a file where the content of an article
 * was previously dumped (using for example OP_SHOWURL, or OP_OPEN with an
 * appropriate "pager" config value ) and checks its content according
 * to the expected values passed as parameters.
 */
inline void AssertArticleFileContent(const std::string& path,
	const std::string& title,
	const std::string& author,
	const std::string& date,
	const std::string& url,
	const std::string& description)
{
	std::string prefix_title = "Title: ";
	std::string prefix_author = "Author: ";
	std::string prefix_date = "Date: ";
	std::string prefix_link = "Link: ";

	std::string line;
	std::ifstream articleFileStream(path);
	REQUIRE(std::getline(articleFileStream, line));
	REQUIRE(line == prefix_title + title);

	REQUIRE(std::getline(articleFileStream, line));
	REQUIRE(line == prefix_author + author);

	REQUIRE(std::getline(articleFileStream, line));
	REQUIRE(line == prefix_date + date);

	REQUIRE(std::getline(articleFileStream, line));
	REQUIRE(line == prefix_link + url);

	REQUIRE(std::getline(articleFileStream, line));
	REQUIRE(line == " ");

	REQUIRE(std::getline(articleFileStream, line));
	REQUIRE(line == description);

	REQUIRE(std::getline(articleFileStream, line));
	REQUIRE(line == "");
};

/* \brief Matcher for an exception with specified message.
 *
 * This helper class can be used with assertion macros like
 * REQUIRE_THROWS_MATCHES:
 *
 *		REQUIRE_THROWS_MATCHES(
 *			call_that_throws(),
 *			exception_type,
 *			ExceptionWithMsg<exception_type>(expected_message));
 */
template<typename Exception>
class ExceptionWithMsg : public Catch::MatcherBase<Exception> {
	std::string expected_msg;

public:
	explicit ExceptionWithMsg(std::string&& msg)
		: expected_msg(std::move(msg))
	{
	}

	explicit ExceptionWithMsg(const std::string& msg)
		: expected_msg(msg)
	{
	}

	bool match(const Exception& e) const override
	{
		return expected_msg == e.what();
	}

	std::string describe() const override
	{
		std::ostringstream ss;
		ss << "should contain message \"" << expected_msg << "\"";
		return ss.str();
	}
};

/* \brief Automatically restores environment variable to its original state
 * when the test finishes running.
 *
 * When an EnvVar object is created, it remembers the state of the given
 * environment variable. After that, the test can safely change the variable
 * directly through setenv(3) and unsetenv(3), or via convenience methods set()
 * and unset(). When the test finishes (with whatever result), the EnvVar
 * restores the variable to its original state.
 *
 * If you need to run some code after the variable is changed (e.g. tzset(3)
 * after a change to TZ), use `on_change()` method to specify a function that
 * should be ran. Note that it will be executed only if the variable is changed
 * through set() and unset() methods; EnvVar won't notice a change made using
 * setenv(3), unsetenv(3), or a change made by someone else (e.g. different
 * thread).
 */
class EnvVar {
	std::function<void(void)> on_change_fn;
	std::string name;
	std::string value;
	bool was_set = false;

public:
	/// \brief Safekeeps the value of environment variable \a name.
	///
	/// \note Accepts a string by value since you'll probably pass it
	/// a temporary or a string literal anyway. Just do it, and use set() and
	/// unset() methods, instead of keeping a local variable with a name in it
	/// and calling setenv(3) and unsetenv(3).
	EnvVar(std::string name_)
		: name(std::move(name_))
	{
		const char* original = ::getenv(name.c_str());
		was_set = original != nullptr;
		if (was_set) {
			value = std::string(original);
		}
	}

	~EnvVar()
	{
		if (was_set) {
			set(value);
		} else {
			unset();
		}
	}

	/// \brief Changes the value of the environment variable.
	///
	/// \note This is equivalent to you calling setenv(3) yourself.
	///
	/// \note This does \emph{not} change the value to which EnvVar will
	/// restore the variable when the test finished running. The variable is
	/// always restored to the state it was in when EnvVar object was
	/// constructed.
	void set(std::string new_value) const
	{
		const auto overwrite = true;
		::setenv(name.c_str(), new_value.c_str(), overwrite);
		if (on_change_fn) {
			on_change_fn();
		}
	}

	/// \brief Unsets the environment variable.
	///
	/// \note This does \emph{not} change the value to which EnvVar will
	/// restore the variable when the test finished running. The variable is
	/// always restored to the state it was in when EnvVar object was
	void unset() const
	{
		::unsetenv(name.c_str());
		if (on_change_fn) {
			on_change_fn();
		}
	}

	/// \brief Specifies a function that should be ran after each call to set()
	/// or unset() methods, and also during object destruction.
	///
	/// In other words, the function will be ran after each change done via or
	/// by this class.
	void on_change(std::function<void(void)> fn)
	{
		on_change_fn = std::move(fn);
	}
};

/// Helper class to create argc and argv arguments for CliArgsParser
///
/// When testing CliArgsParser, resource management turned out to be a problem:
/// CliArgsParser requires char** pointing to arguments, but such a pointer
/// can't be easily obtained from any of standard containers. To overcome that,
/// I wrote Opts, which simply copies elements of initializer_list into
/// separate unique_ptr<char>, and presents useful accessors argc() and argv()
/// whose results can be passed right into CliArgsParser. Problem solved!
class Opts {
	/// Individual elements of argv.
	std::vector<std::unique_ptr<char[]>> m_opts;
	/// This is argv as main() knows it.
	std::unique_ptr<char* []> m_data;
	/// This is argc as main() knows it.
	std::size_t m_argc = 0;

public:
	/// Turns \a opts into argc and argv.
	Opts(std::initializer_list<std::string> opts)
		: m_argc(opts.size())
	{
		m_opts.reserve(m_argc);

		for (const std::string& option : opts) {
			// Copy string into separate char[], managed by
			// unique_ptr.
			auto ptr = std::unique_ptr<char[]>(
				new char[option.size() + 1]);
			std::copy(option.cbegin(), option.cend(), ptr.get());
			// C and C++ require argv to be NULL-terminated:
			// https://stackoverflow.com/questions/18547114/why-do-we-need-argc-while-there-is-always-a-null-at-the-end-of-argv
			ptr.get()[option.size()] = '\0';

			// Hold onto the smart pointer to keep the entry in argv
			// alive.
			m_opts.emplace_back(std::move(ptr));
		}

		// Copy out intermediate argv vector into its final storage.
		m_data = std::unique_ptr<char* []>(new char*[m_argc + 1]);
		int i = 0;
		for (const auto& ptr : m_opts) {
			m_data.get()[i++] = ptr.get();
		}
		m_data.get()[i] = nullptr;
	}

	std::size_t argc() const
	{
		return m_argc;
	}

	char** argv() const
	{
		return m_data.get();
	}
};

} // namespace TestHelpers

#endif /* NEWSBOAT_TEST_HELPERS_H_ */
