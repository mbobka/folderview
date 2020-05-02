// experiment.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <filesystem>
#include <string>
#include <boost/locale.hpp>
using namespace boost::locale;

using namespace std;
using namespace std::filesystem;


generator gen;
std::locale loc = gen("");
vector<wstring> extensions{ L".jpg", L".jpeg" };

void search_inside_directory(vector<path>& folders, vector<path>& files, path root) {
	for (auto entry : directory_iterator(root)) {
  //for (auto entry : recursive_directory_iterator(root)) {
		if (entry.is_directory()) {
			folders.push_back(entry.path());
		}
		else {
			if (entry.is_regular_file()) {
				if (std::find_if(extensions.begin(), extensions.end(), [&entry](wstring& ext) {return 0 == use_facet<collator<wchar_t>>(loc).compare(collator_base::primary, ext, entry.path().extension()); }) != extensions.end())
					files.push_back(entry.path());
			}
		}
	}
}

int main()
{
	std::locale::global(loc);

	auto root = path("D:") / "--- Met-Art.Unpacked ---";
	auto folders = vector<path>();
	auto files = vector<path>();

	search_inside_directory(folders, files, root);

	cout << "Folders: " << folders.size() << endl;
	cout << "Files: " << files.size() << endl;
}


