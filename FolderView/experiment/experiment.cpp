// experiment.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <Windows.h>

using namespace std;
using namespace std::filesystem;


vector<wstring> extensions{ L".jpg", L".jpeg" };

void search_inside_directory(vector<path>& folders, vector<path>& files, path root) {
	for (auto entry : directory_iterator(root)) {
  //for (auto entry : recursive_directory_iterator(root)) {
		if (entry.is_directory()) {
			folders.push_back(entry.path());
		}
		else {
			if (entry.is_regular_file()) {
				if (std::find_if(extensions.begin(), extensions.end(), [&entry](wstring& ext) {return 0 == lstrcmpi(ext.c_str(), entry.path().extension().c_str()); }) != extensions.end())
					files.push_back(entry.path());
			}
		}
	}
}

int main()
{
	auto root = path("\\\\server\\d$\\--- Archive ---\\Matsuura Aya - 2009 - Concert Tour 2009 Aki ~Omoi Afurete~ Visual Book");
//	auto root = path("D:") / "--- Met-Art.Unpacked ---";
	auto folders = vector<path>();
	auto files = vector<path>();

	search_inside_directory(folders, files, root);

	cout << "Folders: " << folders.size() << endl;
	cout << "Files: " << files.size() << endl;
}


