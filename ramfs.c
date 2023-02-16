#include <string.h>
#include "ramfs.h"
#include<stdlib.h>
#include <stdio.h>


typedef struct node{
    enum {FILE_NODE, DIR_NODE } type;
    struct node *dirents[101];
    void *content;
    int nrde;
    int size;
    char *name;
} Node;

typedef struct fd {
    int handle;
    int offset;
    int flags;
    Node *file;
}FD;


Node *root;
int handle_count = 1;
FD *lists[10000];

int judge_name(char *name){
    int len = strlen(name);
    if(name[0]=='/' && len == 1) return 0;
    for (int i = 0; i < len; i++) {
        if(!(name[i]=='.' || (name[i]>=48 && name[i]<=57) ||
             (name[i]>=65 && name[i]<=90) || (name[i]>=97 && name[i]<=122))) return -1;
    }
    return 0;
}

Node *judge_path(const char *pathname,Node *cur,int cut){
    if(cur == NULL) return NULL;
    char jud[31];
    for (int i = 0; i < 31; ++i) {
        jud[i]='\0';
    }
    int len = strlen(pathname) - cut;
    if(cur==root) {
        jud[0] = '/';
        int station = 0;
        for (int i = 0; i < len; ++i) {
            if(pathname[i]!='/'){
                station = 1;
                break;
            }
        }
        if(station == 0) return root;
    }
    int j = 0,i = 0;
    if(jud[0] == 0){
        for (; i < len; i++) {
            if (pathname[i] != '/') jud[j++] = pathname[i];
            else if (j > 0) break;
        }
        if (jud[0] == 0) return root;
    }
    if(strcmp(cur->name,jud)!=0 || judge_name(jud)==-1) return NULL;
    if(cur->type == FILE_NODE && pathname[i] == '/') return NULL;
    j=0;
    for (int k = 0; k < 31; ++k) {
        jud[k]='\0';
    }
    for (; i < len; i++) {
        if(pathname[i] != '/') jud[j++] = pathname[i];
        else if(j > 0) break;
        if(j>=30) return NULL;
    }
    if(judge_name(jud) == -1) return NULL;
    if(i>len) return cur;
    for (int k = 0; k < 101; k++) {
        if(cur->dirents[k]!=NULL){
            if(strcmp(cur->dirents[k]->name,jud)==0){
                cur=cur->dirents[k];
                if(cur->type == FILE_NODE && pathname[i] == '/') return NULL;
                return judge_path(pathname + i,cur,cut);
            }
        }
    }
    return NULL;
}


int ropen(const char *pathname, int flags) {
    char name_[31],name[31];
    for (int i = 0; i < 31; ++i) {
        name[i] = '\0';
        name_[i] = '\0';
    }
    int k = 0;
    for (int i = strlen(pathname) - 1; i >= 0 ; i--) {
        if(pathname[i]!='/') name_[k++] = pathname[i];
        else if(k>0) break;
        if(k>=30) return -1;
    }
    for (int i = 0; i < k; i++) {
        name[i] = name_[k-1-i];
    }
    if(judge_name(name) == -1) return -1;
    Node *tar = judge_path(pathname,root,k);
    if(tar == NULL) return-1;
    Node *cur = tar;
    for (int ii = 0; ii < 101; ii++) {
        if(tar->dirents[ii]!=NULL){
            if(strcmp(tar->dirents[ii]->name,name)==0) {
                cur = tar->dirents[ii];
                break;
            }
        }
    }
    if(cur == tar){
        if(flags>=O_CREAT && flags<=O_TRUNC){
            Node *newnode = malloc(sizeof (Node));
            char *name_cp = malloc(strlen(name)+1);
            strcpy(name_cp,name);
            newnode->name = name_cp;
            newnode->content = NULL;
            newnode->nrde = 0;
            newnode->size = 0;
            newnode->type = FILE_NODE;
            tar->nrde += 1;
            tar->dirents[tar->nrde] = newnode;
            FD *fd = malloc(sizeof (FD));
            fd->flags = flags - O_CREAT;
            fd->offset = 0;
            fd->handle = handle_count;
            fd->file = newnode;
            lists[handle_count] = fd;
            handle_count++;
            return handle_count - 1;
        }
        else return -1;
    }
    else{
        if(cur->type == DIR_NODE) {
            FD *fd = malloc(sizeof (FD));
            fd->handle = handle_count;
            lists[handle_count] = fd;
            handle_count++;
            return handle_count - 1;
        }
        else{
            FD *fd = malloc(sizeof (FD));
            fd->handle = handle_count;
            fd->file = cur;
            lists[handle_count] = fd;
            handle_count++;
            if(flags >= O_APPEND){
                fd->flags = flags - O_APPEND;
                fd->offset = cur->size;
                return handle_count - 1;
            }
            else if (flags == (O_TRUNC | O_WRONLY) ||
                     flags == (O_TRUNC | O_RDWR)){
                fd->flags = flags - O_TRUNC;
                fd->offset = 0;
                fd->file->content = NULL;
                return handle_count - 1;
            }
            else if(flags >= O_CREAT){
                fd->flags = flags - O_CREAT;
                fd->offset = 0;
                return handle_count - 1;
            }
            else if(flags == (O_RDWR | O_WRONLY) || flags==(O_RDONLY | O_WRONLY)){
                fd->flags = O_WRONLY;
                fd->offset = 0;
                return handle_count - 1;
            }
            else if(flags == 0){
                fd->flags = O_RDONLY;
                fd->offset = 0;
                return handle_count - 1;
            }
        }
    }

}

int rclose(int fd) {
    if(fd>handle_count) return -1;
    if(lists[fd] == NULL) return -1;
    if(lists[fd]->handle != 0) lists[fd]->handle = 0;
    else return -1;
    return 0;
}

ssize_t rwrite(int fd, const void *buf, size_t count) {
    FD *ptr = lists[fd];
    Node *file;
    if(ptr != NULL) file = ptr->file;
    else return -1;
    int fl = ptr->flags;
    if(file->type == DIR_NODE ) return -1;
    if(!((fl >= O_WRONLY && fl <= O_RDWR) || fl == (O_WRONLY | O_RDWR)) )return -1;
    int size = ptr->offset + count;
    int size_old = file->size;
    if(size > size_old) {
        void *temp = malloc(size+1);
        memcpy(temp,file->content,size_old);
        file->size = size;
        free(file->content);
        file->content = malloc( size + 1);
        memcpy(file->content,temp,file->size);
    }
    if(ptr->offset > size_old){
        int diff = ptr->offset - size_old;
        char *add = malloc(diff + 1);
        for (int i = 0; i < diff; i++) {
            add[i] = '\0';
        }
        memcpy(file->content + size_old,add,diff);
    }
    void *old = file->content;
    void *new = malloc(size + 1);
    memcpy(new,old,ptr->offset);
    memcpy(new + (ptr->offset),buf,count);
    memcpy(file->content,new,size);
    ptr->offset += count;
    return count;
}

ssize_t rread(int fd, void *buf, size_t count) {
    FD *ptr = lists[fd];
    Node *file;
    if(ptr != NULL) file = ptr->file;
    else return -1;
    if(file->type == DIR_NODE || ptr->flags == O_WRONLY || ptr->flags == (O_RDWR | O_WRONLY)) return -1;
    if((count + ptr->offset) > file->size) {
        count = file->size - ptr->offset;
    }
    memcpy(buf,file->content + ptr->offset,count);
    rseek(fd,count + ptr->offset,0);
    return count;
}

off_t rseek(int fd, off_t offset, int whence) {
    FD *ptr = lists[fd];
    Node *file;
    if(ptr != NULL) file = ptr->file;
    else return -1;
    if(whence == 0){
        if(offset<0) return -1;
        ptr->offset =offset;
    }
    else if(whence == 1){
        ptr->offset += offset;
    }
    else ptr->offset = file->size + offset;
    return ptr->offset;
}

int rmkdir(const char *pathname) {
    if(judge_path(pathname,root,0) != NULL) return -1;
    char name_[31],name[31];
    for (int i = 0; i < 31; ++i) {
        name[i]='\0';
        name_[i]='\0';
    }
    int k = 0;
    for (int i = strlen(pathname) - 1; i >= 0 ; i--) {
        if(pathname[i]!='/') name_[k++] = pathname[i];
        else if(k>0) break;
        if(k>=30) return -1;
    }
    for (int i = 0; i < k; i++) {
        name[i] = name_[k-1-i];
    }
    Node *tar = judge_path(pathname,root,k);
    if(tar == NULL) return -1;
    int i = 0;
    for (; i < 101; i++) {
        if(tar->dirents[i]==NULL) break;
    }
    tar->dirents[i] = malloc(sizeof (Node));
    tar->dirents[i]->type = DIR_NODE;
    char *name_copy = malloc(strlen(name)+1);
    strcpy(name_copy, name);
    tar->dirents[i]->name = name_copy;
    tar->dirents[i]->nrde = 0;
    tar->dirents[i]->size = 0;
    tar->nrde += 1;
    return 0;
}

int rrmdir(const char *pathname) {
    Node *ptr = judge_path(pathname,root,0);
    if(ptr == NULL || ptr->nrde != 0) return -1;
    if(ptr->type == FILE_NODE) return -1;
    char name_[31],name[31];
    int k = 0;
    for (int i = strlen(pathname) - 1; i >= 0 ; i--) {
        if(pathname[i]!='/') name_[k++] = pathname[i];
        else if(k>0) break;
        if(k>=30) return -1;
    }
    for (int i = 0; i < k; i++) {
        name[i] = name_[k-1-i];
    }
    Node *tar = judge_path(pathname,root,k);
    if(tar == NULL) return -1;
    for (int i = 0; i < 101; ++i) {
        if(tar->dirents[i]!=NULL){
            if(strcmp(tar->dirents[i]->name,name)==0){
                tar->dirents[i]=NULL;
                tar->nrde -= 1;
                break;
            }
        }
    }
    return 0;
}

int runlink(const char *pathname) {
    Node *ptr = judge_path(pathname,root,0);
    if(ptr == NULL) return -1;
    if(ptr->type == DIR_NODE) return -1;
    char name_[31],name[31];
    int k = 0;
    for (int i = strlen(pathname) - 1; i >= 0 ; i--) {
        if(pathname[i]!='/') name_[k++] = pathname[i];
        else if(k>0) break;
        if(k>=30) return -1;
    }
    for (int i = 0; i < k; i++) {
        name[i] = name[k-1-i];
    }
    Node *tar = judge_path(pathname,root,k);
    if(tar == NULL) return -1;
    for (int i = 0; i < 101; ++i) {
        if(tar->dirents[i]!=NULL){
            if(strcmp(tar->dirents[i]->name,name)==0){
                tar->dirents[i]=NULL;
                tar->nrde -= 1;
                break;
            }
        }
    }
    return 0;
}

void init_ramfs() {
    root = malloc(sizeof(Node));
    root->type = DIR_NODE;
    root->nrde = 0;
    root->name= "/";
    for (int i = 0; i < 101; i++) {
        root->dirents[i]=NULL;
    }
}
