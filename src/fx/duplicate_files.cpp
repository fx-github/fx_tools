#include "stdafx.h"
#include "duplicate_files.h"


namespace fx
{

const std::size_t DEFAULT_BUFF_SIZE	= 1024;
const std::size_t MAX_BUFF_SIZE		= 64 * 1024 * 1024;

template<typename T>
inline std::vector<T> make_vector(std::initializer_list<T> t_list)
{
	return std::vector<T>(t_list);
}

inline std::size_t double_buff_size(std::size_t& buff_size)
{
	return buff_size = std::min(buff_size * 2, MAX_BUFF_SIZE);
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

inline dbl_vector<const bfs::path*>::iterator append(dbl_vector<const bfs::path*>& dest, const dbl_vector<const bfs::path*>& src)
{
	return dest.insert(dest.cend(), src.cbegin(), src.cend());
}

inline dbl_vector<const bfs::path*>::iterator append(dbl_vector<const bfs::path*>& dest, const cpath_pvect& path_plist)
{
	dest.push_back(path_plist);
	return dest.end();
};

inline dbl_vector<std::string> dbl_string(dbl_vector<const bfs::path*> dbl_vect_ppath)
{
	dbl_vector<std::string> result;
	for (const cpath_pvect& ppath_vect : dbl_vect_ppath)
	{
		string_vect paths;
		for (const bfs::path* ppath : ppath_vect)
			paths.push_back(ppath->string());
		result.push_back(paths);
	}
	return result;
}

size_t path_hash::operator()(const bfs::path& path) const
{
	return std::hash<std::string>()(bfs::system_complete(path).string());
}

/*********************************************************************************/

duplicate_files::duplicate_files()
	: m_recursive(false), m_no_exception(true)
{
}

duplicate_files::duplicate_files(const string_init_list& _input_paths, const string_init_list& _skip_dirs, bool recursive, bool no_exception)
	: m_recursive(recursive), m_no_exception(no_exception)
{
	skip_paths(_skip_dirs);
	input_paths(_input_paths);
}


duplicate_files::~duplicate_files()
{
	assert(check_close_ifs());
	m_input_paths.clear();
	m_skip_paths.clear();
	m_size_map.clear();
	m_ifs_map.clear();
}

bool duplicate_files::check_close_ifs()
{
	bool ret = true;
	for (auto iter = m_ifs_map.begin(); iter != m_ifs_map.end(); ++iter)
		ret &= !iter->second.is_open(), iter->second.close();
	return ret;
}

duplicate_files & duplicate_files::input_paths(const string_vect & _paths)
{
	m_input_paths.clear();

	string_uset paths;
	for (const std::string& path : _paths)
		paths.insert(path);

	for (const std::string& path : paths)
		m_input_paths.push_back(bfs::system_complete(path));
	return *this;
}

string_uset duplicate_files::input_paths()
{
	return string_uset();
}

duplicate_files & duplicate_files::skip_paths(const string_vect & paths)
{
	m_skip_paths.clear();

	for (const std::string& path : paths)
		m_skip_paths.insert(path);
	return *this;
}

string_uset duplicate_files::skip_paths()
{
	return m_skip_paths;
}

duplicate_files & duplicate_files::recursive(bool recursive)
{
	m_recursive = recursive;
	return *this;
}

bool duplicate_files::recursive()
{
	return m_recursive;
}

duplicate_files & duplicate_files::no_exception(bool no_exception)
{
	m_no_exception = no_exception;
	return *this;
}

bool duplicate_files::no_exception()
{
	return m_no_exception;
}

bool duplicate_files::get_files_size_map(size_files_map & size_map)
{
	bool success = true;
	for (const bfs::path& input : m_input_paths)
	{
		bfs::filesystem_error error = get_files_size_map(input, size_map);
		if (error.code().failed())
			success = false, m_no_exception ? (std::cout << error.what() << std::endl, void()) : throw error;
	}
	return success;
}

bfs::filesystem_error duplicate_files::get_files_size_map(const bfs::path& input, size_files_map& size_map)
{
	bfs::path path = bfs::system_complete(input);
	if (!bfs::exists(path))
		return bfs::filesystem_error(std::string(), path, bse::make_error_code(bse::no_such_file_or_directory));

	const bfs::filesystem_error success(std::string(), bse::make_error_code(bse::success));

	if (m_skip_paths.size() > 0 && m_skip_paths.find(path.string()) == m_skip_paths.cend())
		return success;

	if (bfs::is_directory(path))
	{
		if (m_recursive < 0)
			return success;

		m_recursive = m_recursive ? m_recursive : -1;
		for (bfs::directory_iterator iter(path); iter != bfs::end(iter); ++iter)
			get_files_size_map(iter->path(), size_map);
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

bool duplicate_files::compare_two_files(const std::string & _path1, const std::string & _path2)
{
	bfs::path path1 = bfs::system_complete(_path1);
	bfs::path path2 = bfs::system_complete(_path2);
	if (!path1.compare(path2))
		return true;
	if (!bfs::is_regular_file(path1) || !bfs::is_regular_file(path2))
		return false;

	m_ifs_map[&path1].open(path1);
	m_ifs_map[&path2].open(path2);
	bool ret = compare_two_files_p(&path1, &path2).size() > 0;

	m_ifs_map[&path1].close();
	m_ifs_map[&path2].close();
	m_ifs_map.clear();
	return ret;
}

dbl_vector<const bfs::path*> duplicate_files::compare_two_files_p(const bfs::path* path1, const bfs::path* path2)
{
	bfs::ifstream& ifs1 = m_ifs_map[path1];
	bfs::ifstream& ifs2 = m_ifs_map[path2];

	bool result = true;
	std::size_t buff_size = DEFAULT_BUFF_SIZE / 2;
	while (result && !ifs1.eof() && !ifs2.eof())
	{
		double_buff_size(buff_size);
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

	return result ? dbl_vector<const bfs::path*>({ make_vector({ path1 , path2 }) }) : dbl_vector<const bfs::path*>();
}

dbl_vector<std::string> duplicate_files::compare_files()
{
	return dbl_string(compare_files_p(m_input_paths));
}

dbl_vector<const bfs::path*> duplicate_files::compare_files_p(const path_vect& paths)
{
	cpath_pvect		path_plist;
	for (const bfs::path& path : paths)
	{
		path_plist.push_back(&path);
		m_ifs_map[&path].open(path);
	}

	std::size_t buff_size = DEFAULT_BUFF_SIZE;
	typedef std::function<dbl_vector<const bfs::path*>(const cpath_pvect& path_plist)> get_same_files_func;
	get_same_files_func get_same_files = [this, &buff_size, &get_same_files]
	(const cpath_pvect& path_plist) -> dbl_vector<const bfs::path*>
	{
		if (path_plist.size() == 2)
			return compare_two_files_p(path_plist[0], path_plist[1]);
		else if (path_plist.size() <= 1)
			return dbl_vector<const bfs::path*>();

		bool eof = false;
		str_cpaths_map sha1_map;
		char * buff = new char[double_buff_size(buff_size)];
		for (const bfs::path* path : path_plist)
		{
			bfs::ifstream& ifs = m_ifs_map[path];
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

		dbl_vector<const bfs::path*> result;
		for (const auto& pair : sha1_map)
		{
			eof ? append(result, pair.second) : append(result, get_same_files(pair.second));
		}
		return result;
	};

	dbl_vector<const bfs::path*> result = get_same_files(path_plist);
	for (auto iter = m_ifs_map.begin(); iter != m_ifs_map.end(); ++iter)
		iter->second.close();
	m_ifs_map.clear();
	path_plist.clear();
	return result;
}

dbl_vector<std::string> duplicate_files::get()
{
	return dbl_string(get_p());
}

dbl_vector<const bfs::path*> duplicate_files::get_p()
{
	m_size_map.clear();
	get_files_size_map(m_size_map);

	dbl_vector<const bfs::path*> duplicate_files;
	for (const auto& pair : m_size_map)
		append(duplicate_files, compare_files_p(pair.second));
	return duplicate_files;
}

dbl_vector<std::string> duplicate_files::files(const string_init_list& input_paths, bool recursive)
{
	return duplicate_files().input_paths(input_paths).recursive(recursive).get();
}

}
