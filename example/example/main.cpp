//
//  main.cpp
//  example
//
//  Created by Sprax Lines on 9/29/17.
//  Copyright © 2017 Sprax Lines. All rights reserved.
//

#include <iostream>
#include <string>
#include <set>
#include <unordered_set>

using namespace std;

/***********************************************************
Your unordered_map definition should look like this:

unordered_map<
std::string,
std::string,
std::function<unsigned long(std::string)>,
std::function<bool(std::string, std::string)>
> mymap(n, hashing_func, key_equal_fn<std::string>);
The unordered_map is a template and it looks like this:

template<
class Key,
class T,
class Hash = std::hash<Key>,
class KeyEqual = std::equal_to<Key>,
class Allocator = std::allocator<std::pair<const Key, T>>
> class unordered_map;
************************************************************/

static const int MAXWORD = 32;
static const int MAXHASH = 1 << 16;
static char *tab[MAXHASH];

// table-based
struct hashTableChrStr {
    size_t operator()(const char *s) const
    {
        const char *p = s;
        size_t h = 0;
        while (*p) {
            h = 31*h + *p++;
        }
        h %= MAXHASH;
        
        size_t ini = h;
        while (tab[h] && strcmp(tab[h], s)) {
            h = (h + 1) % MAXHASH;
            if (h == ini)
                return -1;
        }
        if (!tab[h])
            tab[h] = strdup(s);
        return h;
    }
};

// djb2 by Dan Bernstein.
struct hashChrStr {
    size_t operator()(const char *str) const
    {
        unsigned long hash = 5381;
        int c;
        
        while ((c = *str++))
            hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
        
        return hash;
    }
};

struct lessCharStrCmp
{
    inline bool operator()(const char* s1, const char* s2) const
    {
        return strcmp(s1, s2) < 0;
    }
};

struct StringHashBySize {
public:
    size_t operator()(const std::string & str) const {
        unsigned long size = str.length();
        return std::hash<unsigned long>()(size);
    }
};

struct eqCharStrCmp
{
    inline bool operator()(const char* s1, const char* s2) const
    {
        return strcmp(s1, s2) == 0;
    }
};

int main(int argc, const char * argv[]) {
    // insert code here...
    cout << "Hello, World!  This is: " << argv[0] << "\n";

    char buffer[MAXWORD] = { '\0' };
    const char *cstr = "dragoon";
    string sstr = cstr;

    unordered_set<const char *> cwords;
    cwords.insert("ago");
    cwords.insert("drag");
    cwords.insert(cstr);
    cwords.insert("dragoons");
    cwords.insert("go");
    cwords.insert("goo");
    cwords.insert("goon");
    cwords.insert("on");
    cwords.insert("rag");

    unordered_set<const char *, hashChrStr, eqCharStrCmp> twords;
    for (const auto& pword: cwords) {                                   // newish range-based for loop
        twords.insert(pword);
    }

    unordered_set<string> swords;
    for (auto itr = twords.cbegin(); itr != twords.cend(); ++itr) {     // traditional iterator-based loop
        swords.insert(*itr);
    }

    cout << "chars strncpy: " << strncpy(buffer, &cstr[2], 3) << endl;
    cout << "Is string literal 'ago' in cwords? " << cwords.count("ago") << endl;
    char ago_a[] = { 'a', 'g', 'o', '\0' };
    cout << "Is separate array 'ago' in cwords? " << cwords.count(ago_a) << endl;

    cout << "chars strncpy: " << strncpy(buffer, &cstr[2], 3) << endl;
    cout << "Is string literal 'ago' in twords? " << twords.count("ago") << endl;
    cout << "Is separate array 'ago' in twords? " << twords.count(ago_a) << endl;

    // TODO: Can we extract a read-only range without creating a new string?
    cout << "string.substr: " << sstr.substr(2, 3) << endl;
    cout << "Is string literal 'ago' in swords? " << swords.count("ago") << endl;
    string ago_s = ago_a;
    cout << "Is another string 'ago' in swords? " << swords.count(ago_s) << endl;


    
    return 0;
}
