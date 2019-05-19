
#include <iostream>
#include <unordered_set>
#include <unordered_map>
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <boost/uuid/detail/sha1.hpp>


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
namespace bfs = boost::filesystem;
namespace bse = boost::system::errc;
namespace bud = boost::uuids::detail;

typedef std::vector<bfs::path>								path_vect;
typedef std::vector<std::string>							string_vect;
typedef std::vector<const bfs::path*>						cpath_pvect;
typedef std::unordered_set<std::string>						string_uset;
typedef std::pair<std::string, std::function<int()>>		option_pair;
typedef std::unordered_map<std::string, cpath_pvect>		str_cpaths_map;
typedef std::unordered_map<boost::uintmax_t, path_vect>		size_files_map;
typedef std::unordered_map<const bfs::path*, bfs::ifstream>	path_ifs_map;

template<class T> using dbl_vector = std::vector<std::vector<T>>;

const char*	APPLICATION_NAME		= "fx_duplicate_files";
const char*	VERSION_NUMBER			= "0.0.0.windows.1";
const std::size_t DEFAULT_BUFF_SIZE	= 1024;
const std::size_t MAX_BUFF_SIZE		= 64 * 1024 * 1024;

namespace key
{
	const std::string help		= "help";
	const std::string skip		= "skip";
	const std::string input		= "input";
	const std::string regex		= "regex";
	const std::string version	= "version";
	const std::string recursive	= "recursive";
}

template<>
struct std::hash<bfs::path>
{
	size_t operator()(const bfs::path& path) const
	{
		return std::hash<std::string>()(bfs::system_complete(path).string());		// todo
	}
};

inline std::string option_name(const std::string& name, const char* short_name = nullptr)
{
	return short_name ? name + "," + short_name : name;
}

template<typename T>
inline std::vector<T> make_vector(std::initializer_list<T> t_list)
{
	return std::vector<T>(t_list);
}

template<class HashAlgo>
std::string hash_to_string(HashAlgo& hash)
{
	typename HashAlgo::digest_type digest;
	hash.get_digest(digest);

	std::stringstream stream;
	for (auto part : digest)
	{
		stream << std::hex << part;
	}
	return stream.str();
}

inline dbl_vector<bfs::path>::iterator append(dbl_vector<bfs::path>& dest, const dbl_vector<bfs::path>& src)
{
	return dest.insert(dest.cend(), src.cbegin(), src.cend());
}

bfs::filesystem_error get_files_size_map(const bfs::path& input, int recursive, const string_uset& skip_dirs, size_files_map& size_map)
{
	bfs::path path = bfs::system_complete(input);
	if (!bfs::exists(path))
		return bfs::filesystem_error(std::string(), path, bse::make_error_code(bse::no_such_file_or_directory));

	const bfs::filesystem_error success(std::string(), bse::make_error_code(bse::success));

	if (skip_dirs.size() > 0 && skip_dirs.find(path.string()) == skip_dirs.cend())
		return success;

	if (bfs::is_directory(path))
	{
		if (recursive < 0)
			return success;

		for (bfs::directory_iterator iter(path); iter != bfs::end(iter); ++iter)
			get_files_size_map(iter->path(), recursive ? recursive : -1, skip_dirs, size_map);
	}
	else
	{
		boost::uintmax_t size = bfs::file_size(path);
		if (size_map.find(size) == size_map.cend())
			size_map[size] = path_vect();
		size_map[size].push_back(path);
	}
	return success;
}

dbl_vector<bfs::path> compare_files(const path_vect& paths)
{
	path_ifs_map	ifs_map;
	cpath_pvect		path_plist;

	for (const bfs::path& path : paths)
	{
		path_plist.push_back(&path);
		ifs_map[&path].open(path);
	}

	auto compare_2files = [&ifs_map](const bfs::path* path1, const bfs::path* path2)
	{
		bfs::ifstream& ifs1 = ifs_map[path1];
		bfs::ifstream& ifs2 = ifs_map[path2];

		bool result = true;
		std::size_t buff_size = DEFAULT_BUFF_SIZE / 2;
		while (result &&!ifs1.eof() && !ifs2.eof())
		{
			DOUBLE_SIZE(buff_size);
			char * buff1 = new char[buff_size];
			char * buff2 = new char[buff_size];
			ifs1.read(buff1, buff_size);
			ifs2.read(buff2, buff_size);
			std::streamsize size1 = ifs1.gcount();
			std::streamsize size2 = ifs2.gcount();
			result = size1 == size2 && !std::memcmp(buff1, buff2, std::size_t(size1));
			delete[] buff1;
			delete[] buff2;
		}
		result = result && ifs1.eof() == ifs2.eof();

		return result ? dbl_vector<bfs::path>({ make_vector({ *path1 , *path2}) }) : dbl_vector<bfs::path>();
	};

	std::size_t buff_size = DEFAULT_BUFF_SIZE;
	typedef std::function<dbl_vector<bfs::path>(const cpath_pvect& path_plist)> get_same_files_func;
	get_same_files_func get_same_files = [&ifs_map, &buff_size, &compare_2files, &get_same_files]
		(const cpath_pvect& path_plist) -> dbl_vector<bfs::path>
	{
		if (path_plist.size() == 2)
			return compare_2files(path_plist[0], path_plist[1]);
		else if (path_plist.size() <= 1)
			return dbl_vector<bfs::path>();

		bool eof = false;
		str_cpaths_map sha1_map;
		char * buff = new char[DOUBLE_SIZE(buff_size)];
		for (const bfs::path* path : path_plist)
		{
			bfs::ifstream& ifs = ifs_map[path];
			ifs.read(buff, buff_size);
			if (ifs.gcount() > 0)
			{
				bud::sha1 sha1;
				sha1.process_bytes(buff, std::size_t(ifs.gcount()));
				std::string hash = hash_to_string(sha1);
				if (sha1_map.find(hash) == sha1_map.cend())
					sha1_map.insert(std::make_pair(hash, cpath_pvect()));
				sha1_map[hash].push_back(path);
			}
			else
			{
				sha1_map["0"].push_back(path);
			}
			eof = ifs.eof();
		}
		delete[] buff;

		auto append_func = [](dbl_vector<bfs::path>& dest, const cpath_pvect& path_plist)
		{
			path_vect path_list;
			for (const bfs::path* path : path_plist)
			{
				path_list.push_back(*path);
			}
			dest.push_back(path_list);
			return dest.end();
		};

		dbl_vector<bfs::path> result;
		for (const auto& pair : sha1_map)
		{
			eof ? append_func(result, pair.second) : append(result, get_same_files(pair.second));
		}
		return result;
	};

	dbl_vector<bfs::path> result = get_same_files(path_plist);

	for (auto iter = ifs_map.begin(); iter != ifs_map.end(); ++iter)
		iter->second.close();
	ifs_map.clear();
	path_plist.clear();

	return result;
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
	bool recursive = vm.count(key::recursive);
	string_vect input_dirs = vm[key::input].as<string_vect>();
	string_uset skip_dirs;
	if (vm.count(key::skip))
	{
		for (const std::string& path : vm[key::skip].as<string_vect>())
			skip_dirs.insert(bfs::system_complete(path).string());
	}

	size_files_map size_map;
	for (const std::string& input : input_dirs)
	{
		bfs::filesystem_error error = get_files_size_map(input, recursive, skip_dirs, size_map);
		if (error.code().failed())
			std::cout << error.what() << std::endl;
	}

	std::vector<path_vect> duplicate_files;
	for (const auto& pair : size_map)
	{
		append(duplicate_files, compare_files(pair.second));
	}

	for (std::size_t i = 0; i < duplicate_files.size(); ++i)
	{
		std::cout << "duplicate files " << i + 1 << ":" << std::endl;
		for (const bfs::path& path : duplicate_files[i])
			std::cout << "\t" << path.string() << std::endl;
		std::cout << std::endl;
	}
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
		(OPTION_NAME(key::recursive,"r"), "recurse directories.")
		(OPTION_NAME(key::regex,	"e"), bpo::value<string_vect>()->value_name("PATTERN"), "use PATTERN for matching.")
		(OPTION_NAME(key::input,	"i"), bpo::value<string_vect>()->value_name("DIRECTORIES"), "input DIRECTORIES.")
		(OPTION_NAME(key::skip,		"s"), bpo::value<string_vect>()->value_name("DIRECTORIES"), "skip/ignore DIRECTORIES.")
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
		vm.insert(std::make_pair(key::input, bpo::variable_value(boost::any(string_vect({ "." })), false)));

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

