
/*  
 *  $Id$
 *
 *                              
 *                    L O S  A L A M O S
 *              Los Alamos National Laboratory
 *               Los Alamos, New Mexico 87545
 *                                  
 *  Copyright, 1986, The Regents of the University of California.
 *                                  
 *           
 *	Author Jeffrey O. Hill
 *	johill@lanl.gov
 *	505 665 1831
 *
 * NOTES:
 * 1) This interface is preliminary and will change in the future
 */


#include "dbNotify.h"
#include "dbEvent.h"
#include "dbAddr.h"
#include "dbLock.h"
#include "dbCommon.h"
#include "db_convert.h"
#include "resourceLib.h"

extern "C" void putNotifyCompletion ( putNotify *ppn );

class dbServiceIO;
class dbChannelIO;
class dbPutNotifyBlocker;
class dbSubscriptionIO;

class dbBaseIO : public chronIntIdRes < dbBaseIO > {
public:
    virtual dbSubscriptionIO * isSubscription () = 0;
    virtual void destroy () = 0;
    virtual void show ( unsigned level ) const = 0;
};

class dbPutNotifyBlocker : public dbBaseIO {
public:
    dbPutNotifyBlocker ( dbChannelIO &chanIn );
    void initiatePutNotify ( epicsMutex &mutex, cacWriteNotify &notify, struct dbAddr &addr, 
            unsigned type, unsigned long count, const void *pValue );
    void cancel ();
    void show ( unsigned level ) const;
    void * operator new ( size_t size );
    void operator delete ( void *pCadaver, size_t size );
    void destroy ();
protected:
    virtual ~dbPutNotifyBlocker ();
private:
    putNotify pn;
    epicsEvent block;
    dbChannelIO &chan;
    cacWriteNotify *pNotify;
    dbSubscriptionIO * isSubscription ();
    static tsFreeList < dbPutNotifyBlocker > freeList;
    static epicsMutex freeListMutex;
    friend void putNotifyCompletion ( putNotify *ppn );
};

extern "C" void dbSubscriptionEventCallback ( void *pPrivate, struct dbAddr *paddr,
	int eventsRemaining, struct db_field_log *pfl );

class dbSubscriptionIO : public tsDLNode <dbSubscriptionIO>, public dbBaseIO {
public:
    dbSubscriptionIO ( dbServiceIO &, dbChannelIO &, struct dbAddr &, cacStateNotify &, 
        unsigned type, unsigned long count, unsigned mask, cacChannel::ioid * );
    void destroy ();
    void channelDestroyException ();
    void show ( unsigned level ) const;
    void * operator new ( size_t size );
    void operator delete ( void *pCadaver, size_t size );
protected:
    virtual ~dbSubscriptionIO ();
private:
    cacStateNotify & notify;
    dbChannelIO & chan;
    dbEventSubscription es;
    unsigned type;
    unsigned long count;
    unsigned id;
    dbSubscriptionIO * isSubscription ();
    static tsFreeList < dbSubscriptionIO > freeList;
    static epicsMutex freeListMutex;
    friend void dbSubscriptionEventCallback ( void *pPrivate, struct dbAddr *paddr,
	    int eventsRemaining, struct db_field_log *pfl );
};

class dbServiceIO;

class dbServicePrivateListOfIO {
public:
    dbServicePrivateListOfIO ();
private:
    tsDLList < dbSubscriptionIO > eventq;
    dbPutNotifyBlocker *pBlocker;
    friend class dbServiceIO;
};

class dbChannelIO : public cacChannel, public dbServicePrivateListOfIO {
public:
    dbChannelIO ( cacChannelNotify &notify, 
        const dbAddr &addr, dbServiceIO &serviceIO );
    void destroy ();
    void callReadNotify ( unsigned type, unsigned long count, 
            cacReadNotify &notify );
    void callStateNotify ( unsigned type, unsigned long count, 
            const struct db_field_log *pfl, cacStateNotify &notify );
    void show ( unsigned level ) const;
    void * operator new ( size_t size);
    void operator delete ( void *pCadaver, size_t size );
    const char *pName () const;
protected:
    ~dbChannelIO (); // allocate only from pool
private:
    dbServiceIO & serviceIO;
    dbAddr addr;
    void initiateConnect ();
    ioStatus read ( unsigned type, unsigned long count, 
        cacReadNotify &, ioid * );
    void write ( unsigned type, unsigned long count, 
        const void *pvalue );
    ioStatus write ( unsigned type, unsigned long count, 
        const void *pvalue, cacWriteNotify &, ioid * );
    void subscribe ( unsigned type, unsigned long count, 
        unsigned mask, cacStateNotify &notify, ioid * );
    void ioCancel ( const ioid & );
    void ioShow ( const ioid &, unsigned level ) const;
    short nativeType () const;
    unsigned long nativeElementCount () const;
    static tsFreeList < dbChannelIO > freeList;
    static epicsMutex freeListMutex;
    static unsigned nextIdForIO;
};

class dbServiceIO : public cacService {
public:
    dbServiceIO ();
    virtual ~dbServiceIO ();
    cacChannel * createChannel ( const char *pName, 
                    cacChannelNotify &, cacChannel::priLev );
    void callReadNotify ( struct dbAddr &addr, unsigned type, unsigned long count, 
            cacReadNotify &notify );
    void callStateNotify ( struct dbAddr &addr, unsigned type, unsigned long count, 
            const struct db_field_log *pfl, cacStateNotify &notify );
    dbEventSubscription subscribe ( struct dbAddr & addr, dbChannelIO & chan,
        dbSubscriptionIO & subscr, unsigned mask );
    void initiatePutNotify ( dbChannelIO &, struct dbAddr &, unsigned type, 
        unsigned long count, const void *pValue, cacWriteNotify &notify, 
        cacChannel::ioid *pId ); 
    void show ( unsigned level ) const;
    void showAllIO ( const dbChannelIO &chan, unsigned level ) const;
    void destroyAllIO ( dbChannelIO & chan );
    void ioCancel ( dbChannelIO & chan, const cacChannel::ioid &id );
    void ioShow ( const cacChannel::ioid &id, unsigned level ) const;
private:
    chronIntIdResTable < dbBaseIO > ioTable;
    unsigned long eventCallbackCacheSize;
    dbEventCtx ctx;
    char *pEventCallbackCache;
    mutable epicsMutex mutex;
};

inline dbServicePrivateListOfIO::dbServicePrivateListOfIO () :
    pBlocker ( 0 )
{
}
