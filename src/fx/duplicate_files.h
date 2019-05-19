#ifndef __FX_DUPLICATE_FILES_H__
#define __FX_DUPLICATE_FILES_H__


namespace fx
{
namespace bfs = boost::filesystem;
namespace bse = boost::system::errc;
namespace bud = boost::uuids::detail;

struct path_hash;

typedef std::vector<bfs::path>								path_vect;
typedef std::vector<std::string>							string_vect;
typedef std::vector<const bfs::path*>						cpath_pvect;
typedef std::unordered_set<std::string>						string_uset;
typedef std::initializer_list<std::string>					string_init_list;
typedef std::unordered_map<std::string, cpath_pvect>		str_cpaths_map;
typedef std::unordered_map<boost::uintmax_t, path_vect>		size_files_map;
typedef std::unordered_map<const bfs::path*, bfs::ifstream>	path_ifs_map;
typedef std::unordered_set<bfs::path, path_hash>			path_uset;

template<class T> using dbl_vector = std::vector<std::vector<T>>;

struct path_hash
{
	size_t operator()(const bfs::path& path) const;
};

class duplicate_files
{
public:
	duplicate_files();
	duplicate_files(const string_init_list& input_paths, const string_init_list& skip_paths = {}, bool recursive = false, bool no_exception = false);
	~duplicate_files();

	duplicate_files&	input_paths(const string_vect& paths);
	string_uset			input_paths();
	duplicate_files&	skip_paths(const string_vect& paths);
	string_uset			skip_paths();
	duplicate_files&	recursive(bool recursive);
	bool				recursive();
	duplicate_files&	no_exception(bool no_exception);
	bool				no_exception();

	bool compare_two_files(const std::string& path1, const std::string& path2);
	dbl_vector<std::string> compare_files();
	dbl_vector<std::string> get();

	static dbl_vector<std::string> files(const string_init_list& input_paths, bool recursive = false);

private:
	bool get_files_size_map(size_files_map& size_map);
	bfs::filesystem_error get_files_size_map(const bfs::path& input_path, size_files_map& size_map);
	dbl_vector<const bfs::path*> compare_two_files_p(const bfs::path* path1, const bfs::path* path2);
	dbl_vector<const bfs::path*> compare_files_p(const path_vect& paths);
	dbl_vector<const bfs::path*> get_p();

	bool check_close_ifs();

private:
	int				m_recursive;
	bool			m_no_exception;
	path_vect		m_input_paths;
	string_uset		m_skip_paths;
	size_files_map	m_size_map;
	path_ifs_map	m_ifs_map;
};

}

#endif // !__FX_DUPLICATE_FILES_H__
