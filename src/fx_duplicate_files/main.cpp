// fx_duplicate_files.cpp: 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include "duplicate_files.h"

#define FX_EXIT EXIT;

#define FX_CHECK(exp)	\
	if (!(exp))			\
		goto EXIT;

#define FX_TRY(statement)	\
	try						\
	{						\
		statement;			\
	}

#define FX_CATCH_EXCEPTION_EXIT(exception_type)				\
	catch (exception_type e)								\
	{														\
		std::cout << (e).what() << std::endl << std::endl;	\
		goto EXIT;											\
	}

#define OPTION_NAME(key, short_name)	option_name((key), (short_name)).c_str()
#define DOUBLE_SIZE(buff_size)			(buff_size) = std::min((buff_size) * 2, MAX_BUFF_SIZE)

namespace bpo = boost::program_options;

typedef std::pair<std::string, std::function<int()>>		option_pair;

const char*	APPLICATION_NAME	= "fxduplicatefiles";
const char*	VERSION_NUMBER		= "0.0.0.windows.1";

namespace key
{
	const std::string help		= "help";
	const std::string skip		= "skip";
	const std::string input		= "input";
	const std::string regex		= "regex";
	const std::string version	= "version";
	const std::string recursive	= "recursive";
}

inline std::string option_name(const std::string& name, const char* short_name = nullptr)
{
	return short_name ? name + "," + short_name : name;
}

int option_version_function()
{
	std::cout << APPLICATION_NAME << " version " << VERSION_NUMBER << std::endl;
	return 0;
}

int option_help_function(const bpo::options_description & desc)
{
	std::cout << desc << std::endl;
	return 0;
}

int option_input_function(const bpo::variables_map * p_vm)
{
	if (!p_vm)
		return -1;

	const bpo::variables_map& vm = *p_vm;
	fx::duplicate_files df;
	if (vm.count(key::skip))
		df.skip_paths(vm[key::skip].as<fx::string_vect>());
	df.recursive(vm.count(key::recursive));

	fx::string_vect input_dirs = vm[key::input].as<fx::string_vect>();
	std::vector<fx::string_vect> duplicate_files = df.input_paths(input_dirs).get();

	for (std::size_t i = 0; i < duplicate_files.size(); ++i)
	{
		std::cout << "duplicate files " << i + 1 << ":" << std::endl;
		for (const std::string& path : duplicate_files[i])
			std::cout << "\t" << path << std::endl;
		std::cout << std::endl;
	}

	std::cout << std::endl;
	duplicate_files = df.compare_files();
	for (std::size_t i = 0; i < duplicate_files.size(); ++i)
	{
		std::cout << "duplicate files " << i + 1 << ":" << std::endl;
		for (const std::string& path : duplicate_files[i])
			std::cout << "\t" << path << std::endl;
		std::cout << std::endl;
	}

	boost::filesystem::current_path("D:\\code");
	std::cout << df.compare_two_files("", "") << std::endl;
	std::cout << df.compare_two_files("", "1") << std::endl;
	std::cout << df.compare_two_files("", "a.txt") << std::endl;
	std::cout << df.compare_two_files("a.txt", "a.txt") << std::endl;
	std::cout << df.compare_two_files("a.txt", "b.txt") << std::endl;
	std::cout << df.compare_two_files("a.txt", "a - 副本.txt") << std::endl;

	return 0;
}

int main(int argc, char * argv[])
{
	bpo::options_description mDesc("miscellaneous");
	mDesc.add_options()
		(OPTION_NAME(key::version, "v"), "display version information and exit.")
		(OPTION_NAME(key::help, "h"), "display this help text and exit.")
// 		;
// 	options_description ocDesc("output control:");
// 	ocDesc.add_options()
		(OPTION_NAME(key::recursive, "r"), "recurse directories.")
		(OPTION_NAME(key::regex, "e"), bpo::value<fx::string_vect>()->value_name("PATTERN"), "use PATTERN for matching.")
		(OPTION_NAME(key::input, "i"), bpo::value<fx::string_vect>()->value_name("DIRECTORIES"), "input DIRECTORIES.")
		(OPTION_NAME(key::skip, "s"), bpo::value<fx::string_vect>()->value_name("DIRECTORIES"), "skip/ignore DIRECTORIES.")
		;

	bpo::positional_options_description posDesc;
	posDesc.add(key::input.c_str(), -1);

	bpo::variables_map vm;
	std::vector<option_pair> option_functions =
	{
		{ key::version,	option_version_function },
		{ key::help,	std::bind(option_help_function, mDesc) },
		{ key::input,	std::bind(option_input_function, &vm) },
	};

	bpo::command_line_parser parser = bpo::command_line_parser(argc, argv).options(mDesc)/*.options(ocDesc)*/.positional(posDesc)
		.style(bpo::command_line_style::default_style | bpo::command_line_style::allow_slash_for_short);
// 	boost::program_options::parse_command_line(argc, argv, mDesc);

	FX_TRY(store(parser.run(), vm))
	FX_CATCH_EXCEPTION_EXIT(bpo::error_with_option_name)
	FX_CATCH_EXCEPTION_EXIT(std::exception);

	if (!vm.count(key::input))
		vm.insert(std::make_pair(key::input, bpo::variable_value(boost::any(fx::string_vect({ "." })), false)));

	for (option_pair pair : option_functions)
	{
		if (vm.count(pair.first))
			return pair.second();
	}

EXIT:
	std::cout << "usage: " << APPLICATION_NAME << " [OPTION]..." << std::endl;
	std::cout << "try '" << APPLICATION_NAME << " --help' for more information." << std::endl;
	return -1;
}

