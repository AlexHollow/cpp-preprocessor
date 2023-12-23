#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

using namespace std;
using filesystem::path;

path operator""_p(const char* data, std::size_t sz) {
    return path(data, data + sz);
}

bool Preprocess(const path& in_file, const path& out_file, const vector<path>& include_directories) {
    const regex r_1(R"(\s*#\s*include\s*<([^>]*)>\s*)");
    const regex r_2(R"(\s*#\s*include\s*\"([^"]*)\"\s*)");

    ifstream fin(in_file);
    if (!fin) { return false; }

    ofstream fout(out_file, ios::app);
    if (!fout) { return false; }

    int line_count = 0;

    while (fin) {
        string str;
        getline(fin, str);
        ++line_count;

        smatch m;
        path matched_p;

        if (regex_match(str, m, r_1)) {
            bool is_exist = false;
            matched_p = m[1].str();

            for (const auto& dir : include_directories) {
                path p = dir / matched_p;
                if (filesystem::exists(p)) {
                    is_exist = true;
                    Preprocess(p, out_file, include_directories);
                    break;
                }
            }

            if (!is_exist) {
                cout << "unknown include file "s << m[1] << " at file "s << in_file.string() << " at line "s << line_count << endl;
                return false;
            }
        } else if (regex_match(str, m, r_2)) {
            matched_p = m[1].str();
            path p = in_file.parent_path() / matched_p.parent_path() / matched_p.filename();

            if (filesystem::exists(p)) {
                Preprocess(p, out_file, include_directories);
            } else {
                bool is_exist = false;

                for (const auto& dir : include_directories) {
                    path under_p = dir / matched_p.parent_path() / matched_p.filename();
                    if (filesystem::exists(under_p)) {
                        is_exist = true;
                        Preprocess(under_p, out_file, include_directories);
                        break;
                    }
                }

                if (!is_exist) {
                    cout << "unknown include file "s << m[1] << " at file "s << in_file.string() << " at line "s << line_count << endl;
                    return false;
                }
            }
        }

        if (fin && !regex_match(str, m, r_1) && !regex_match(str, m, r_2)) {
            fout << str << endl;
        }
    }

    fin.close();
    fout.close();

    return true;
}

string GetFileContents(string file) {
    ifstream stream(file);

    return { (istreambuf_iterator<char>(stream)), istreambuf_iterator<char>() };
}

void Test() {
    error_code err;
    filesystem::remove_all("sources"_p, err);
    filesystem::create_directories("sources"_p / "include2"_p / "lib"_p, err);
    filesystem::create_directories("sources"_p / "include1"_p, err);
    filesystem::create_directories("sources"_p / "dir1"_p / "subdir"_p, err);

    {
        ofstream file("sources/a.cpp");
        file << "// this comment before include\n"
            "#include \"dir1/b.h\"\n"
            "// text between b.h and c.h\n"
            "#include \"dir1/d.h\"\n"
            "\n"
            "int SayHello() {\n"
            "    cout << \"hello, world!\" << endl;\n"
            "#   include<dummy.txt>\n"
            "}\n"s;
    }
    {
        ofstream file("sources/dir1/b.h");
        file << "// text from b.h before include\n"
            "#include \"subdir/c.h\"\n"
            "// text from b.h after include"s;
    }
    {
        ofstream file("sources/dir1/subdir/c.h");
        file << "// text from c.h before include\n"
            "#include <std1.h>\n"
            "// text from c.h after include\n"s;
    }
    {
        ofstream file("sources/dir1/d.h");
        file << "// text from d.h before include\n"
            "#include \"lib/std2.h\"\n"
            "// text from d.h after include\n"s;
    }
    {
        ofstream file("sources/include1/std1.h");
        file << "// std1\n"s;
    }
    {
        ofstream file("sources/include2/lib/std2.h");
        file << "// std2\n"s;
    }

    assert((!Preprocess("sources"_p / "a.cpp"_p, "sources"_p / "a.in"_p,
        { "sources"_p / "include1"_p,"sources"_p / "include2"_p })));

    ostringstream test_out;
    test_out << "// this comment before include\n"
        "// text from b.h before include\n"
        "// text from c.h before include\n"
        "// std1\n"
        "// text from c.h after include\n"
        "// text from b.h after include\n"
        "// text between b.h and c.h\n"
        "// text from d.h before include\n"
        "// std2\n"
        "// text from d.h after include\n"
        "\n"
        "int SayHello() {\n"
        "    cout << \"hello, world!\" << endl;\n"s;

    assert(GetFileContents("sources/a.in"s) == test_out.str());
}

int main() {
    Test();
}