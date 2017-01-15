#include "stdafx.h"

using namespace std;

enum diff_options {
	diffNone = 0x0,
	diffOld = 0x1,
	diffNew = 0x2,
	diffHelp = 0x80000000,
};

const struct { const wchar_t* arg; const wchar_t* arg_alt; const wchar_t* params_desc; const wchar_t* description; const diff_options options; } cmd_options[] = {
	{ L"?",		L"help",			nullptr,		L"show this help",						diffHelp },
	{ L"n",		L"new",				L"<filename>",	L"specify new file(s)",					diffNew },
	{ L"o",		L"old",				L"<filename>",	L"specify old file(s)",					diffOld },
};

void print_usage() {
	printf_s("\tUsage: resdiff [options]\n\n");
	for (auto o = std::begin(cmd_options); o != std::end(cmd_options); ++o) {
		if (o->arg != nullptr) printf_s("\t-%S", o->arg); else printf_s("\t");

		int len = 0;
		if (o->arg_alt != nullptr) {
			len = wcslen(o->arg_alt);
			printf_s("\t--%S", o->arg_alt);
		}
		else printf_s("\t");

		if (len < 6) printf_s("\t");

		if (o->params_desc != nullptr) len += printf_s(" %S", o->params_desc);

		if (len < 14) printf_s("\t");

		printf_s("\t: %S\n", o->description);
	}
}

map<wstring, wstring> find_files(const wchar_t* pattern);

struct resource {
	bool loaded;
	map<wstring, map<wstring, vector<unsigned char>>> data;
};
resource load_resource(const wstring& file);
void diff_strings(const wstring& name, const vector<unsigned char> * new_data, const vector<unsigned char> * old_data);

int wmain(int argc, wchar_t* argv[])
{
	int options = diffNone;
	const wchar_t* err_arg = nullptr;
	wstring new_files_pattern, old_files_pattern;

	printf_s("\n ResDiff v0.1 https://github.com/WalkingCat/ResDiff\n\n");

	for (int i = 1; i < argc; ++i) {
		const wchar_t* arg = argv[i];
		if ((arg[0] == '-') || ((arg[0] == '/'))) {
			diff_options curent_option = diffNone;
			if ((arg[0] == '-') && (arg[1] == '-')) {
				for (auto o = std::begin(cmd_options); o != std::end(cmd_options); ++o) {
					if ((o->arg_alt != nullptr) && (_wcsicmp(arg + 2, o->arg_alt) == 0)) { curent_option = o->options; }
				}
			}
			else {
				for (auto o = std::begin(cmd_options); o != std::end(cmd_options); ++o) {
					if ((o->arg != nullptr) && (_wcsicmp(arg + 1, o->arg) == 0)) { curent_option = o->options; }
				}
			}

			bool valid = false;
			if (curent_option != diffNone) {
				valid = true;
				if (curent_option == diffNew) {
					if ((i + 1) < argc) new_files_pattern = argv[++i];
					else valid = false;
				}
				else if (curent_option == diffOld) {
					if ((i + 1) < argc) old_files_pattern = argv[++i];
					else valid = false;
				}
				else options = (options | curent_option);
			}
			if (!valid && (err_arg == nullptr)) err_arg = arg;
		}
		else { if (new_files_pattern.empty()) new_files_pattern = arg; else err_arg = arg; }
	}

	if ((new_files_pattern.empty() && old_files_pattern.empty()) || (err_arg != nullptr) || (options & diffHelp)) {
		if (err_arg != nullptr) printf_s("\tError in option: %S\n\n", err_arg);
		print_usage();
		return 0;
	}

	auto new_files = find_files(new_files_pattern.c_str());
	auto old_files = find_files(old_files_pattern.c_str());

	printf_s(" new files: %S%S\n", new_files_pattern.c_str(), !new_files.empty() ? L"" : L" (NOT EXISTS!)");
	printf_s(" old files: %S%S\n", old_files_pattern.c_str(), !old_files.empty() ? L"" : L" (NOT EXISTS!)");

	printf_s("\n");

	if (new_files.empty() & old_files.empty()) return 0; // at least one of them must exists

	printf_s(" diff legends: +: added, -: removed, *: changed, $: changed (original)\n");
	printf_s("\n");

	for (auto& new_pair : new_files) {
		auto& new_file = new_pair.second;
		auto old_file_it = old_files.find(new_pair.first);
		if (old_file_it == old_files.end()) {
			if ((new_files.size() == 1) && (old_files.size() == 1)) {
				old_file_it = old_files.begin();
			} else {
				printf_s("\n+ FILE: %ws\n", new_file.c_str());
				continue;
			}
		}
		auto& old_file = old_file_it->second;

		auto old_res = load_resource(old_file);
		auto new_res = load_resource(new_file);

		for (auto& new_type_pair : new_res.data) {
			auto& type_name = new_type_pair.first;
			auto& new_type = new_type_pair.second;
			auto& old_type = old_res.data[type_name];
			for (auto& new_data_pair : new_type) {
				auto& name = new_data_pair.first;
				const vector<unsigned char> * new_data = &new_data_pair.second;
				const vector<unsigned char> * old_data = nullptr;
				bool diff = false;
				if (old_type.find(name) == old_type.end()) {
					printf_s(" + %ws # %ws\n", type_name.c_str(), name.c_str());
					diff = true;
				} else {
					old_data = &old_type[name];
					if ((new_data->size() != old_data->size()) || (memcmp(new_data->data(), old_data->data(), new_data->size()) != 0)) {
						printf_s(" * %ws # %ws\n", type_name.c_str(), name.c_str());
						diff = true;
					}
				}
				if (diff) {
					if (type_name == L"STRING") {
						diff_strings(name, new_data, old_data);
					}
				}
			}
		}
		for (auto& old_type_pair : old_res.data) {
			auto& type_name = old_type_pair.first;
			auto& old_type = old_type_pair.second;
			auto& new_type = new_res.data[type_name];
			for (auto& old_data_pair : old_type) {
				auto& name = old_data_pair.first;
				auto& old_data = old_data_pair.second;
				if (new_type.find(old_data_pair.first) == new_type.end()) {
					printf_s(" - %ws # %ws\n", type_name.c_str(), name.c_str());
				}
			}
		}
	}

    return 0;
}

map<wstring, wstring> find_files(const wchar_t * pattern)
{
	map<wstring, wstring> ret;
	wchar_t path[MAX_PATH] = {};
	wcscpy_s(path, pattern);
	WIN32_FIND_DATA fd;
	HANDLE find = ::FindFirstFile(pattern, &fd);
	if (find != INVALID_HANDLE_VALUE) {
		do {
			if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
				PathRemoveFileSpec(path);
				PathCombine(path, path, fd.cFileName);
				ret[fd.cFileName] = path;
			}
		} while (::FindNextFile(find, &fd));
		::FindClose(find);
	}
	return ret;
}

resource load_resource(const wstring& file) {
	resource ret;
	auto module = LoadLibraryExW(file.c_str(), NULL, LOAD_LIBRARY_AS_IMAGE_RESOURCE);
	ret.loaded = (module != NULL);
	if (ret.loaded) {
		EnumResourceTypesW(module, [](HMODULE hModule, LPTSTR lpszType, LONG_PTR lParam) -> BOOL {
			auto& rsrc = *(resource*)lParam;
			bool ignore = false;
			if (IS_INTRESOURCE(lpszType)) {
				switch ((int)lpszType) {
				case (int)RT_VERSION:		ignore = true; break;
				case (int)RT_MANIFEST:		ignore = true; break;
				}
			} else {
				if (wcscmp(lpszType, L"MUI") == 0) ignore = true;
			}
			if (!ignore) {
				EnumResourceNamesW(hModule, lpszType, [](HMODULE hModule, LPCTSTR lpszType, LPTSTR lpszName, LONG_PTR lParam) -> BOOL {
					wstring type_name, name;
					
					if (IS_INTRESOURCE(lpszType)) {
						switch ((int)lpszType) {
						case (int)RT_CURSOR:		type_name = L"CURSOR"; break;
						case (int)RT_BITMAP:		type_name = L"BITMAP"; break;
						case (int)RT_ICON:			type_name = L"ICON"; break;
						case (int)RT_MENU:			type_name = L"MENU"; break;
						case (int)RT_DIALOG:		type_name = L"DIALOG"; break;
						case (int)RT_STRING:		type_name = L"STRING"; break;
						case (int)RT_FONTDIR:		type_name = L"FONTDIR"; break;
						case (int)RT_FONT:			type_name = L"FONT"; break;
						case (int)RT_ACCELERATOR:	type_name = L"ACCELERATOR"; break;
						case (int)RT_RCDATA:		type_name = L"RCDATA"; break;
						case (int)RT_MESSAGETABLE:	type_name = L"MESSAGETABLE"; break;
						case (int)RT_GROUP_CURSOR:	type_name = L"GROUP_CURSOR"; break;
						case (int)RT_GROUP_ICON:	type_name = L"GROUP_ICON"; break;
						case (int)RT_HTML:			type_name = L"HTML"; break;
						default: {
								wchar_t buf[16] = {};
								_itow_s((int)lpszType, buf, 10);
								type_name = buf;
								break;
							}
						}
					} else {
						type_name = wstring(L"\"") + lpszType + L"\"";
					}

					if (IS_INTRESOURCE(lpszName)) {
						wchar_t buf[16] = {};
						_itow_s((int)lpszName, buf, 10);
						name = buf;
					} else name = wstring(L"\"") + lpszName + L"\"";
					auto hrsrc = FindResourceW(hModule, lpszName, lpszType);
					if (hrsrc != NULL) {
						auto res = LockResource(LoadResource(hModule, hrsrc));
						auto size = SizeofResource(hModule, hrsrc);
						auto& rsrc = *(resource*)lParam;
						auto& data = rsrc.data[type_name][name];
						data.resize(size);
						memcpy_s(data.data(), data.size(), res, size);
					}
					return TRUE;
				}, (LONG_PTR)&rsrc);
			}
			return TRUE;
		}, (LONG_PTR) &ret);
		FreeLibrary(module);
	}
	return ret;
}

void diff_strings(const wstring& name, const vector<unsigned char> * new_data, const vector<unsigned char> * old_data) {
	const WORD id_hi = (WORD) wcstoul(name.c_str(), nullptr, 10) - 1;
	auto parse_strings = [&id_hi](const vector<unsigned char> * data) -> map<WORD, wstring> {
		map<WORD, wstring> ret;
		if (data != nullptr) {
			const size_t size = data->size();
			size_t pos = 0;
			for (WORD n = 0; n < 16; ++n) {
				const WORD id = (id_hi << 4) + n;
				if ((pos + sizeof(WORD)) <= size) {
					WORD len = *(WORD*)(data->data() + pos);
					pos += sizeof(WORD);
					if (len > 0) {
						if ((pos + len * sizeof(wchar_t)) <= size) {
							ret.emplace(make_pair(id, wstring((wchar_t*)(data->data() + pos), len)));
							pos += len * 2;
						} else break;
					} else continue;
				} else break;
			}
		}
		return ret;
	};
	
	auto new_strings = parse_strings(new_data);
	auto old_strings = parse_strings(old_data);

	for (auto& pair : new_strings) {
		auto& id = pair.first;
		auto& new_str = pair.second;
		if (old_strings.find(id) == old_strings.end()) {
			printf_s("  + %d %ws\n", id, new_str.c_str());
		} else {
			auto& old_str = old_strings[id];
			if (new_str != old_str) {
				printf_s("  * %d %ws\n", id, new_str.c_str());
				printf_s("  $ %d %ws\n", id, old_str.c_str());
			}
		}
	}
	for (auto& pair : old_strings) {
		auto& id = pair.first;
		auto& old_str = pair.second;
		if (new_strings.find(id) == new_strings.end()) {
			printf_s("  - %d %ws\n", id, old_str.c_str());
		}
	}
}