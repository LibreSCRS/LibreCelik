// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me
// Code taken and adapted from Ludovic Rousseau's pcsc_scan.c

#include "smartcardscanner.h"
#include "smartcardevent.h"
#include "utils/libreceliklog.h"
#include "smartcardscanner.h"
#include "PCSC/winscard.h"
#include "PCSC/wintypes.h"
#include <sysexits.h>
#include <QThread>

#define TIMEOUT 3600*1000	/* 1 hour timeout */

SmartCardScanner::SmartCardScanner(QObject *parent)
    : QObject{parent}
{

}

void SmartCardScanner::requestStop()
{
    qCDebug(librecSCRSCard) << "SmartCardWorker: Requesting stop!";
    SCardCancel(hContext);
}

#define test_rv_establish(fct, rv, hContext) \
do { \
        if (rv != SCARD_S_SUCCESS) \
    { \
            qCDebug(librecSCRSCard) << "FCT: " << fct " RV: " << rv; \
            (void)SCardReleaseContext(hContext); \
            throw std::runtime_error("Cannot establish context in SmartCardScanner"); \
    } \
} while(0)

#define test_rv(fct, rv, hContext) \
do { \
        if (rv != SCARD_S_SUCCESS) \
    { \
            qCDebug(librecSCRSCard) << "FCT: " << fct " RV: " << rv; \
            (void)SCardReleaseContext(hContext); \
            rv = SCardEstablishContext(SCARD_SCOPE_SYSTEM, NULL, NULL, &hContext); \
            test_rv_establish("SCardEstablishContext", rv, hContext); \
    } \
} while(0)

// TODO: This has to block on PCSC and emit appropriate signals.
// SCardCancel()
// To force SCardGetStatusChange() to return before the end of the timeout you must use SCardCancel() from another thread of the application.
// You can use a very long timeout (or even the special value INFINITE) and use SCardCancel() when needed.
// TODO: Refactor and get rid of gotos. Investigate potential memory leaks. Code has not been thoroughly tested.
void SmartCardScanner::doWork()
{
    int current_reader;
#ifdef __APPLE__
    uint32_t rv;
#else
    LONG rv;
#endif
    SCARD_READERSTATE *rgReaderStates_t = NULL;
    SCARD_READERSTATE rgReaderStates[1];
    DWORD dwReaders = 0, dwReadersOld;
    LPSTR mszReaders = NULL;
    char *ptr = NULL;
    const char **readers = NULL;
    int nbReaders, i;
    int pnp = true;

    rv = SCardEstablishContext(SCARD_SCOPE_SYSTEM, NULL, NULL, &hContext);
    test_rv_establish("SCardEstablishContext", rv, hContext);

    rgReaderStates[0].szReader = "\\\\?PnP?\\Notification";
    rgReaderStates[0].dwCurrentState = SCARD_STATE_UNAWARE;

    SCardGetStatusChange(hContext, 0, rgReaderStates, 1);
    if (rgReaderStates[0].dwEventState & SCARD_STATE_UNKNOWN)
    {
        qCDebug(librecSCRSCard) << "SmartCardScanner: PNP not available";
        pnp = false;
    }
    else
    {
        qCDebug(librecSCRSCard) << "SmartCardScanner: Using PNP";
    }

// Check if interrupt is requested
get_readers:
    while(!QThread::currentThread()->isInterruptionRequested())
    {
        /* free memory possibly allocated in a previous loop */
        if (NULL != readers)
        {
            free(readers);
            readers = NULL;
        }

        if (NULL != rgReaderStates_t)
        {
            free(rgReaderStates_t);
            rgReaderStates_t = NULL;
        }

        /* Retrieve the available readers list.
         *
         * 1. Call with a null buffer to get the number of bytes to allocate
         * 2. malloc the necessary storage
         * 3. call with the real allocated buffer
        */
        qCDebug(librecSCRSCard) << "SmartCardScanner: Scanning present readers...";
        rv = SCardListReaders(hContext, NULL, NULL, &dwReaders);
        if (rv != SCARD_E_NO_READERS_AVAILABLE)
            test_rv("SCardListReaders", rv, hContext);

        dwReadersOld = dwReaders;

        /* if non NULL we came back so free first */
        if (mszReaders)
        {
            free(mszReaders);
            mszReaders = NULL;
        }

        mszReaders = static_cast<char*>(malloc(sizeof(char)*dwReaders));
        if (mszReaders == NULL)
        {
            qCCritical(librecSCRSCard) << "SmartCardScanner: malloc: not enough memory";
            exit(EX_OSERR);
        }

        *mszReaders = '\0';
        rv = SCardListReaders(hContext, NULL, mszReaders, &dwReaders);

        /* Extract readers from the null separated string and get the total number of readers */
        nbReaders = 0;
        ptr = mszReaders;
        while (*ptr != '\0')
        {
            ptr += strlen(ptr)+1;
            nbReaders++;
        }

        QStringList scrNames;

        if (SCARD_E_NO_READERS_AVAILABLE == rv || 0 == nbReaders)
        {
            qCDebug(librecSCRSCard) << "Waiting for the first reader...";

            // Emit that there are no card readers and wait for them to appear
            emit smartCardReaderEnumerationChanged(scrNames);
            if (pnp)
            {
                rgReaderStates[0].szReader = "\\\\?PnP?\\Notification";
                rgReaderStates[0].dwCurrentState = SCARD_STATE_UNAWARE;
                do
                {
                    rv = SCardGetStatusChange(hContext, TIMEOUT, rgReaderStates, 1);
                }
                while (SCARD_E_TIMEOUT == rv);
                test_rv("SCardGetStatusChange", rv, hContext);
            }
            else
            {
                rv = SCARD_S_SUCCESS;
                while ((SCARD_S_SUCCESS == rv) && (dwReaders == dwReadersOld))
                {
                    rv = SCardListReaders(hContext, NULL, NULL, &dwReaders);
                    if (SCARD_E_NO_READERS_AVAILABLE == rv)
                        rv = SCARD_S_SUCCESS;
                    QThread::sleep(1);
                    if (QThread::currentThread()->isInterruptionRequested())
                    {
                        return; // TODO: ??
                    }
                }
            }
            qCDebug(librecSCRSCard) << "Found one. Scaning again...";
            continue;
        }
        else
            test_rv("SCardListReader", rv, hContext);

        /* allocate the readers table */
        readers = static_cast<const char**>(calloc(nbReaders+1, sizeof(char *)));
        if (NULL == readers)
        {
            qCCritical(librecSCRSCard) << "SmartCardScanner: Not enough memory for readers table.";
            exit(EX_OSERR);
        }

        /* fill the readers table */
        nbReaders = 0;
        ptr = mszReaders;
        while (*ptr != '\0')
        {
            readers[nbReaders] = ptr;
            ptr += strlen(ptr)+1;
            nbReaders++;
        }

        /* allocate the ReaderStates table */
        rgReaderStates_t = static_cast<SCARD_READERSTATE*>(calloc(nbReaders+1, sizeof(* rgReaderStates_t)));
        if (NULL == rgReaderStates_t)
        {
            qCCritical(librecSCRSCard) << "SmartCardScanner: Not enough memory for reader states table.";
            (void)SCardReleaseContext(hContext);
            exit(EX_OSERR);
        }

        /* Set the initial states to something we do not know
         * The loop below will include this state to the dwCurrentState
         */
        for (i=0; i<nbReaders; i++)
        {
            scrNames << readers[i];
            rgReaderStates_t[i].szReader = readers[i];
            rgReaderStates_t[i].dwCurrentState = SCARD_STATE_UNAWARE;
            rgReaderStates_t[i].cbAtr = sizeof rgReaderStates_t[i].rgbAtr;
        }

        /* If Plug and Play is supported by the PC/SC layer */
        if (pnp)
        {
            rgReaderStates_t[nbReaders].szReader = "\\\\?PnP?\\Notification";
            rgReaderStates_t[nbReaders].dwCurrentState = SCARD_STATE_UNAWARE;
            nbReaders++;
        }



        // Emit current card readers name and wait for events on them
        emit smartCardReaderEnumerationChanged(scrNames);


        /* Wait endlessly for all events in the list of readers. We only stop in case of an error */
        rv = SCardGetStatusChange(hContext, TIMEOUT, rgReaderStates_t, nbReaders);



        while ((rv == SCARD_S_SUCCESS) || (rv == SCARD_E_TIMEOUT))
        {
            time_t t;
            if (pnp)
            {
                /* check if the number of readers has changed */
                if (rgReaderStates_t[nbReaders-1].dwEventState & SCARD_STATE_CHANGED)
                {
                    goto get_readers;
                }
            }
            else
            {
                /* A new reader appeared? */
                if ((SCardListReaders(hContext, NULL, NULL, &dwReaders)
                     == SCARD_S_SUCCESS) && (dwReaders != dwReadersOld))
                {
                    goto get_readers;
                }
            }
            if (rv != SCARD_E_TIMEOUT)
            {
                /* Timestamp the event as we get notified */
                t = time(NULL);
                qCDebug(librecSCRSCard) << "SmartCardScanner: Event " << rv << "Time: " << ctime(&t);
            }

            /* Now we have an event, check all the readers in the list to see what happened */
            for (current_reader=0; current_reader < nbReaders; current_reader++)
            {
#if defined(__APPLE__) || defined(WIN32)
                if (rgReaderStates_t[current_reader].dwCurrentState == rgReaderStates_t[current_reader].dwEventState)
                    continue;
#endif
                if (rgReaderStates_t[current_reader].dwEventState & SCARD_STATE_CHANGED)
                {
                    /* If something has changed the new state is now the current state */
                    rgReaderStates_t[current_reader].dwCurrentState = rgReaderStates_t[current_reader].dwEventState;
                }
                else
                    /* If nothing changed then skip to the next reader */
                    continue;

                /* From here we know that the state for the current reader has
                 * changed because we did not pass through the continue statement
                 * above.
                 */
                /* Specify the current reader's number and name */
                qCDebug(librecSCRSCard) << "SmartCardScanner: Reader " << current_reader << " State: " << rgReaderStates_t[current_reader].szReader;

                /* Event number */
                qCDebug(librecSCRSCard) << "SmartCardScanner: Event number:  " << (rgReaderStates_t[current_reader].dwEventState >> 4);

                /* Dump the full current state */
                qCDebug(librecSCRSCard) << "SmartCardScanner: Card state: ";
                if (rgReaderStates_t[current_reader].dwEventState & SCARD_STATE_IGNORE)
                    qCDebug(librecSCRSCard) << "SmartCardScanner:     Ignore this reader, ";

                if (rgReaderStates_t[current_reader].dwEventState & SCARD_STATE_UNKNOWN)
                {
                    qCDebug(librecSCRSCard) << "SmartCardScanner:     Unknown";
                    emit smartCardEventOccured({rgReaderStates_t[current_reader].szReader, SmartCardEvent::EventType::CardRemoved});
                    goto get_readers;
                }

                SmartCardEvent::EventType eventType = SmartCardEvent::Unknown;
                if (rgReaderStates_t[current_reader].dwEventState & SCARD_STATE_UNAVAILABLE)
                    qCDebug(librecSCRSCard) << " SmartCardScanner:    Status unavailable";

                if (rgReaderStates_t[current_reader].dwEventState & SCARD_STATE_EMPTY)
                {
                    qCDebug(librecSCRSCard) << "SmartCardScanner:     Card removed";
                    eventType = SmartCardEvent::EventType::CardRemoved;
                }

                if (rgReaderStates_t[current_reader].dwEventState & SCARD_STATE_PRESENT)
                {
                    if (rgReaderStates_t[current_reader].dwEventState & SCARD_STATE_EXCLUSIVE)
                    {
                        qCDebug(librecSCRSCard) << "SmartCardScanner:     Exclusive Mode";
                        continue;
                    }
                    else if (rgReaderStates_t[current_reader].dwEventState & SCARD_STATE_INUSE)
                    {
                        qCDebug(librecSCRSCard) << "SmartCardScanner:     Shared Mode";
                        continue;
                    }
                    else if (rgReaderStates_t[current_reader].dwEventState & SCARD_STATE_MUTE)
                    {
                        qCDebug(librecSCRSCard) << "SmartCardScanner:     Unresponsive card";
                        continue;
                    }
                    else
                    {
                        qCDebug(librecSCRSCard) << "SmartCardScanner:     Card inserted";
                        eventType = SmartCardEvent::EventType::CardInserted;
                    }
                }

                if (rgReaderStates_t[current_reader].dwEventState & SCARD_STATE_ATRMATCH)
                    qCDebug(librecSCRSCard) << "SmartCardScanner:     ATR matches card";

                if (rgReaderStates_t[current_reader].dwEventState & SCARD_STATE_UNPOWERED)
                    qCDebug(librecSCRSCard) << "SmartCardScanner:     Unpowered card";

                emit smartCardEventOccured({rgReaderStates_t[current_reader].szReader, eventType});
            } /* for */

            rv = SCardGetStatusChange(hContext, TIMEOUT, rgReaderStates_t, nbReaders);
        } /* while */

        /* PCSC became unavailable */
        if (rv == SCARD_E_NO_SERVICE)
        {
            qCDebug(librecSCRSCard) << "SmartCardScanner: SCARD_E_NO_SERVICE";

            /* Cleanup all readers before re-establishing context */
            for (const auto& reader : scrNames)
                emit smartCardEventOccured({reader.toStdString(), SmartCardEvent::EventType::CardRemoved});

            /* Try to re-establish context */
            test_rv("SCardGetStatusChange", rv, hContext);
            continue;
        }

        /* A reader disappeared */
        if (SCARD_E_UNKNOWN_READER == rv)
            continue;

        /* If we get out the loop, GetStatusChange() was unsuccessful */
        if (rv != SCARD_E_CANCELLED)
            test_rv("SCardGetStatusChange", rv, hContext);
        else
        {
            qCDebug(librecSCRSCard) << "SmartCardScanner: SCARD_E_CANCELLED";
        }
    } // while not interrupted
    qCDebug(librecSCRSCard) << "SmartCardScanner: QThread::currentThread interrupted!";

    /* We try to leave things as clean as possible */
    rv = SCardReleaseContext(hContext);
    test_rv("SCardReleaseContext", rv, hContext);

    /* free memory possibly allocated */
    if (NULL != readers)
        free(readers);
    if (NULL != rgReaderStates_t)
        free(rgReaderStates_t);
}
