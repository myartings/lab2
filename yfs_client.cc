// yfs client.  implements FS operations using extent and lock server
#include "yfs_client.h"
#include "extent_client.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
using namespace std; // My change

yfs_client::yfs_client()
{
    ec = new extent_client();

}

yfs_client::yfs_client(std::string extent_dst, std::string lock_dst)
{
    ec = new extent_client();
    if (ec->put(1, "") != extent_protocol::OK)
        printf("error init root dir\n"); // XYB: init root dir
}

yfs_client::inum
yfs_client::n2i(std::string n)
{
    std::istringstream ist(n);
    unsigned long long finum;
    ist >> finum;
    return finum;
}

std::string
yfs_client::filename(inum inum)
{
    std::ostringstream ost;
    ost << inum;
    return ost.str();
}

bool
yfs_client::isfile(inum inum)
{
    extent_protocol::attr a;

    if (ec->getattr(inum, a) != extent_protocol::OK) {
        printf("error getting attr\n");
        return false;
    }

    if (a.type == extent_protocol::T_FILE) {
        printf("isfile: %lld is a file\n", inum);
        return true;
    } 
    printf("isfile: %lld is a dir\n", inum);
    return false;
}

bool
yfs_client::isdir(inum inum)
{
    return ! isfile(inum);
}

int
yfs_client::getfile(inum inum, fileinfo &fin)
{
    int r = OK;

    printf("getfile %016llx\n", inum);
    extent_protocol::attr a;
    if (ec->getattr(inum, a) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }

    fin.atime = a.atime;
    fin.mtime = a.mtime;
    fin.ctime = a.ctime;
    fin.size = a.size;
    printf("getfile %016llx -> sz %llu\n", inum, fin.size);

release:
    return r;
}

int
yfs_client::getdir(inum inum, dirinfo &din)
{
    int r = OK;

    printf("getdir %016llx\n", inum);
    extent_protocol::attr a;
    if (ec->getattr(inum, a) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }
    din.atime = a.atime;
    din.mtime = a.mtime;
    din.ctime = a.ctime;

release:
    return r;
}


#define EXT_RPC(xx) do { \
    if ((xx) != extent_protocol::OK) { \
        printf("EXT_RPC Error: %s:%d \n", __FILE__, __LINE__); \
        r = IOERR; \
        goto release; \
    } \
} while (0)

// Only support set size of attr
int
yfs_client::setattr(inum ino, size_t size)
{
    int r = OK;

    /*
     * your lab2 code goes here.
     * note: get the content of inode ino, and modify its content
     * according to the size (<, =, or >) content length.
     */
    //My change
    string data;
    ec->get(ino, data);
    size_t size_old = data.size();
    if(size <= size_old)
    {
	data = data.substr(0, size);
    }
    else
    {
	data.resize(size, '\0');
    }

    ec->put(ino, data);
    //end change

    return r;
}

int
yfs_client::create(inum parent, const char *name, mode_t mode, inum &ino_out)
{
    int r = OK;

    /*
     * your lab2 code goes here.
     * note: lookup is what you need to check if file exist;
     * after create file or dir, you must remember to modify the parent infomation.
     */

    //My change
    bool found;
    lookup(parent, name, found, ino_out);
    if(!found)
    {
	//extent_protocol::extentid_t id;
	if(mode == (S_IFDIR |0777))
        	ec->create(extent_protocol::T_DIR, ino_out); 
	else
	    ec->create(extent_protocol::T_FILE, ino_out);
	string s;
	s = string(name) + "\t" + filename(ino_out) + "\t";
	fileinfo fin;
	getfile(parent, fin);
	size_t bytes_written;
	write(parent, s.size(), fin.size, s.c_str(), bytes_written);
    }
    else r = EXIST;
  
    //end change

    return r;
}

int
yfs_client::lookup(inum parent, const char *name, bool &found, inum &ino_out)
{
    int r = OK;

    /*
     * your lab2 code goes here.
     * note: lookup file from parent dir according to name;
     * you should design the format of directory content.
     */

    //My change
    string content;
    std::list<dirent> list;
    readdir(parent, list);
    std::list<dirent>::iterator it;
    for(it = list.begin(); it != list.end(); it++)
    {
	if(!(*it).name.compare(name))
	{
	    ino_out = (*it).inum;
	    found = true;
	    r = EXIST;
            return r;
	}
    }

    found = false;
    
    //end change

    return r;
}

int
yfs_client::readdir(inum dir, std::list<dirent> &list)
{
    int r = OK;

    /*
     * your lab2 code goes here.
     * note: you should parse the dirctory content using your defined format,
     * and push the dirents to the list.
     */

    //My change
    string dir_data;
    fileinfo fin;
    getfile(dir ,fin); 
    read(dir, fin.size, 0, dir_data);
    string buf;
    stringstream ss(dir_data);
    vector<string> tokens;
    while(ss >> buf)
	tokens.push_back(buf);
     
    struct dirent dir_entry;
    std::vector<string>::iterator it;
    for(it = tokens.begin(); it != tokens.end(); it++)
    {
	dir_entry.name = *it++;
	dir_entry.inum = n2i(*it);
	list.push_back(dir_entry);
    }
    //end change

    return r;
}

int
yfs_client::read(inum ino, size_t size, off_t off, std::string &data)
{
    int r = OK;

    /*
     * your lab2 code goes here.
     * note: read using ec->get().
     */

    //My change
    string temp_str;
    ec->get(ino, temp_str);
    if((size_t)off >= temp_str.size())
	return r;
    else
    {
	size_t size_read = size < temp_str.size() - off ? size : temp_str.size() - off;
	data = temp_str.substr(off, size_read);
    }
    

    //end change
    return r;
}

int
yfs_client::write(inum ino, size_t size, off_t off, const char *data,
        size_t &bytes_written)
{
    int r = OK;

    /*
     * your lab2 code goes here.
     * note: write using ec->put().
     * when off > length of original file, fill the holes with '\0'.
     */
    
    //My change
    string temp_str;
    fileinfo fin;
    size_t size_tmp;
    getfile(ino, fin);
    size_tmp = fin.size;
    read(ino, size_tmp, 0, temp_str);
    
	/*for(int i = 0; i< temp_str.size()/4096; i++)
	{
    string stemp = temp_str.substr(i*4096,100);
    printf("ino is %d, size is %d, off is %d, read content is %s \n", ino, size, off, stemp.c_str());
	stemp = temp_str.substr((i+1)*4096- 100, 100);
    printf("ino is %d, size is %d, off is %d, read content is %s \n", ino, size, off, stemp.c_str());
	printf("/////////////////////////////////////////////////////\n");

	}
	string temp = data;
	if(size >1024)
	{
	    string s = temp.substr(0, 100);
    printf("ino is %d, size is %d, off is %d, write content is %s \n", ino, size, off, s.c_str()); 
	      s = temp.substr(size-100, 100);
    printf("ino is %d, size is %d, off is %d, write content is %s \n", ino, size, off, s.c_str());
             printf("__________________________________________________________\n");
	}*/

    if((size_t)off > size_tmp)
    {
	temp_str.resize(off, '\0');
	temp_str.append(data, 0, size);
	bytes_written = off - size_tmp + size;
    }
    else
    {
	if(size_tmp - (size_t)off < size)
	{
	    temp_str = temp_str.substr(0, off);
	    temp_str.append(data, 0, size);
	}
	else
	{
	    string data_str = data;
	    data_str = data_str.substr(0, size);
	    temp_str = temp_str.replace(off, size, data_str);
	}
	    bytes_written = size;
    }
    	/*for(int i = 0; i< temp_str.size()/4096; i++)
	{
    string stemp = temp_str.substr(i*4096,100);
    printf("ino is %d, size is %d, off is %d, final content is %s \n", ino, size, off, stemp.c_str());
	stemp = temp_str.substr((i+1)*4096- 100, 100);
    printf("ino is %d, size is %d, off is %d, final content is %s \n", ino, size, off, stemp.c_str());
	printf("/////////////////////////////////////////////////////\n");

	}*/
    ec->put(ino, temp_str);
    //end change

    return r;
}

int yfs_client::unlink(inum parent,const char *name)
{
    int r = OK;

    /*
     * your lab2 code goes here.
     * note: you should remove the file using ec->remove,
     * and update the parent directory content.
     */

    //My change
    bool found = false;
    yfs_client::inum ino;
    lookup(parent, name, found, ino); 
    if(!found) return 1;
    if(isdir(ino)) return 2;
    
    ec->remove(ino);
    std::list<dirent> list;
    readdir(parent, list);
    std::list<dirent>::iterator it;
    for(it = list.begin(); it != list.end(); it++)
    {
	if((*it).inum == ino)
	    break;
    }
    list.erase(it);

    string dir_data;
    for(it = list.begin(); it!= list.end(); it++)
    {
        dir_data = dir_data + (*it).name + "\t" + filename((*it).inum) + "\t";
    }
    ec->put(parent, dir_data);
    //end change

    return r;
}

