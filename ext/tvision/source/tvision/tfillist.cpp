/*------------------------------------------------------------*/
/* filename -       tfillist.cpp                              */
/*                                                            */
/* function(s)                                                */
/*                  TFileList member functions                */
/*------------------------------------------------------------*/
/*
 *      Turbo Vision - Version 2.0
 *
 *      Copyright (c) 1994 by Borland International
 *      All Rights Reserved.
 *
 */

#define Uses_TVMemMgr
#define Uses_MsgBox
#define Uses_TFileList
#define Uses_TRect
#define Uses_TSearchRec
#define Uses_TEvent
#define Uses_TGroup
#define Uses_TKeys
#include <tvision/tv.h>

#if !defined( __DIR_H )
#include <dir.h>
#endif  // __DIR_H

#if !defined( __ERRNO_H )
#include <errno.h>
#endif  // __ERRNO_H

#if !defined( __STDIO_H )
#include <stdio.h>
#endif  // __STDIO_H

#if !defined( __CTYPE_H )
#include <ctype.h>
#endif  // __CTYPE_H

#if !defined( __ASSERT_H )
#include <assert.h>
#endif  // __ASSERT_H

#if !defined( __DOS_H )
#include <dos.h>
#endif  // __DOS_H

#if !defined( __STRING_H )
#include <string.h>
#endif  // __STRING_H

#if defined( __FLAT__ ) && defined( __BORLANDC__)
extern "C" char _FAR * _CType _FARFUNC strupr(char _FAR *__s);
#endif


TFileList::TFileList( const TRect& bounds,
                      TScrollBar *aScrollBar) :
    TSortedListBox( bounds, 2, aScrollBar )
{
}

TFileList::~TFileList()
{
   if ( list() )
      destroy ( list() );
}

void TFileList::focusItem( short item )
{
    TSortedListBox::focusItem( item );
    message( owner, evBroadcast, cmFileFocused, list()->at(item) );
}

void TFileList::selectItem( short item )
{
    message( owner, evBroadcast, cmFileDoubleClicked, list()->at(item) );
}

void TFileList::getData( void * )
{
}

void TFileList::setData( void * )
{
}

ushort TFileList::dataSize()
{
    return 0;
}

void* TFileList::getKey( const char *s )
{
static TSearchRec sR;

    if( (shiftState & kbShift) != 0 || *s == '.' )
        sR.attr = FA_DIREC;
    else
        sR.attr = 0;
    strcpy( sR.name, s );
    strupr( sR.name );
    return &sR;
}

void TFileList::getText( char *dest, short item, short maxChars )
{
    TSearchRec *f = (TSearchRec *)(list()->at(item));

    strncpy( dest, f->name, maxChars );
    dest[maxChars] = '\0';
    if( f->attr & FA_DIREC )
        strcat( dest, "\\" );
}


void TFileList::readDirectory( TStringView dir, TStringView wildCard )
{
    char path[MAXPATH];
    size_t n = strnzcpy( path, dir, MAXPATH );
    strnzcpy( &path[n], wildCard, MAXPATH - n );
    readDirectory( path );
}

struct DirSearchRec : public TSearchRec
{
    void readFf_blk(ffblk *f)
    {
        attr = (char)f->ff_attrib;
        time = (((long)(unsigned)f->ff_fdate)<<16) | f->ff_ftime;
        size = f->ff_fsize;
        memcpy(name, f->ff_name, sizeof(f->ff_name));
    }

    void *operator new( size_t );

};

void *DirSearchRec::operator new( size_t sz )
{
    void *temp = ::operator new( sz );
#ifdef __BORLANDC__ // Would work anyway, but it's unnecessary.
    if( TVMemMgr::safetyPoolExhausted() )
        {
        delete temp;
        temp = 0;
        }
#endif
    return temp;
}

void TFileList::readDirectory( TStringView aWildCard )
{
    ffblk s;
    char path[MAXPATH];
    char drive[MAXDRIVE];
    char dir[MAXDIR];
    char file[MAXFILE];
    char ext[MAXEXT];
    const unsigned findAttr = FA_RDONLY | FA_ARCH;
    memset(&s, 0, sizeof(s));
    strnzcpy( path, aWildCard, MAXPATH );

    TFileCollection *fileList = new TFileCollection( 5, 5 );

    int res = findfirst( path, &s, findAttr );
    DirSearchRec *p = (DirSearchRec *)&p;
    while( p != 0 && res == 0 )
        {
        if( (s.ff_attrib & FA_DIREC) == 0 )
            {
            p = new DirSearchRec;
            if( p != 0 )
                {
                p->readFf_blk(&s);
                fileList->insert( p );
                }
            }
        res = findnext( &s );
        }

    fexpand( path );
    fnsplit( path, drive, dir, file, ext );
    fnmerge( path, drive, dir, "*", ".*" );

    res = findfirst( path, &s, FA_DIREC );
    while( p != 0 && res == 0 )
        {
        if( (s.ff_attrib & FA_DIREC) != 0 && s.ff_name[0] != '.' )
            {
            p = new DirSearchRec;
            if( p != 0 )
                {
                p->readFf_blk(&s);
                fileList->insert( p );
                }
            }
        res = findnext( &s );
        }

    if( strlen( dir ) > 1 )
        {
        p = new DirSearchRec;
        if( p != 0 )
            {
            fnmerge( path, drive, dir, "..", 0 );
            if( findfirst( path, &s, FA_DIREC ) == 0 )
                {
                strcpy( s.ff_name, ".." ); // findfirst returns the actual
                p->readFf_blk(&s);         // directory name.
                }
            else
                {
                strcpy( p->name, ".." );
                p->size = 0;
                p->time = 0x210000uL;
                p->attr = FA_DIREC;
                }
            fileList->insert( p );
            }
        }

    if( p == 0 )
        messageBox( tooManyFiles, mfOKButton | mfWarning );
    newList(fileList);
    if( list()->getCount() > 0 )
        message( owner, evBroadcast, cmFileFocused, list()->at(0) );
    else
        {
        static DirSearchRec noFile;
        message( owner, evBroadcast, cmFileFocused, &noFile );
        }
}

/*
    fexpand:    reimplementation of pascal's FExpand routine.  Takes a
                relative DOS path and makes an absolute path of the form

                    drive:\[subdir\ ...]filename.ext

                works with '/' or '\' as the subdir separator on input;
                changes all to '\' on output.

*/

#pragma warn -inl

inline static void skip( char *&src, char k )
{
    while( *src == k )
        src++;
}

#pragma warn .inl

void squeeze( char *path )
{
    char *dest = path;
    char *src = path;
    char last = '\0';
    while( *src != 0 )
        {
        if( last == '\\' )
            skip( src, '\\' );  // skip repeated '\'
        if( (!last || last == '\\') && *src == '.' )
            {
            src++;
            if( !*src || *src == '\\' ) // have a '.' or '.\'
                skip( src, '\\' );
            else if( *src == '.' && (!src[1] || src[1] == '\\'))
                {               // have a '..' or '..\'
                src++;          // skip the following '.'
                skip( src, '\\' );
                dest--;         // back up to the previous '\'
                while( dest > path && *--dest != '\\' ) // back up to the previous '\'
                    ;
                dest++;         // move to the next position
                }
            else
                last = *dest++ = src[-1]; // copy the '.' we just skipped
            }
        else
            last = *dest++ = *src++;   // just copy it...
        }
    *dest = EOS;                // zero terminator
}

void fexpand( char *rpath )
{
    char path[MAXPATH];
    char drive[MAXDRIVE];
    char dir[MAXDIR];
    char file[MAXFILE];
    char ext[MAXEXT];

    int flags = fnsplit( rpath, drive, dir, file, ext );
    if( (flags & DRIVE) == 0 )
        {
        drive[0] = getdisk() + 'A';
        drive[1] = ':';
        drive[2] = '\0';
        }
    drive[0] = toupper(drive[0]);
    if( (flags & DIRECTORY) == 0 || (dir[0] != '\\' && dir[0] != '/') )
        {
        char curdir[MAXDIR+1];
        if ( getcurdir( drive[0] - 'A' + 1, curdir ) != 0 )
            *curdir = '\0';
        strnzcat( curdir, "\\", MAXDIR+1 );
        strnzcat( curdir, dir, MAXDIR+1 );
        if( *curdir != '\\' && *curdir != '/' )
            {
            *dir = '\\';
            strnzcpy( dir+1, curdir, MAXDIR-1 );
            }
        else
            strnzcpy( dir, curdir, MAXDIR );
        }

    char *p = dir;
    while( (p = strchr( p, '/' )) != 0 )
        *p = '\\';
    squeeze( dir );
    fnmerge( path, drive, dir, file, ext );
#ifndef __FLAT__
    strupr( path );
#endif
    strnzcpy( rpath, path, MAXPATH );
}

#if !defined(NO_STREAMABLE)

TStreamable *TFileList::build()
{
    return new TFileList( streamableInit );
}

#endif
