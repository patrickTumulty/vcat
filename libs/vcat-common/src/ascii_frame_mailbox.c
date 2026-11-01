
#include "ascii_frame_mailbox.h"
#include <pthread.h>
#include <stdlib.h>

/// One node per frame sitting in the free pool.
typedef struct AsciiFrameNode
{
    matrix_t frame;
    struct AsciiFrameNode *next;
} AsciiFrameNode;

struct AsciiFrameMailbox
{
    pthread_mutex_t availableMutex;
    AsciiFrameNode *availableHead;
    AsciiFrameNode *availableTail;
    pthread_mutex_t latestMutex;
    matrix_t latest;
};

AsciiFrameMailbox *vcatAsciiFrameMailboxCreate(void)
{
    AsciiFrameMailbox *mailbox = calloc(1, sizeof(*mailbox));
    if (mailbox == NULL)
    {
        return NULL;
    }

    if (pthread_mutex_init(&mailbox->availableMutex, NULL) != 0 || pthread_mutex_init(&mailbox->latestMutex, NULL) != 0)
    {
        free(mailbox);
        return NULL;
    }

    return mailbox;
}

void vcatAsciiFrameMailboxFree(AsciiFrameMailbox *mailbox)
{
    if (mailbox == NULL)
    {
        return;
    }

    pthread_mutex_lock(&mailbox->latestMutex);
    matrix_free(mailbox->latest);
    mailbox->latest = NULL;
    pthread_mutex_unlock(&mailbox->latestMutex);

    pthread_mutex_lock(&mailbox->availableMutex);
    AsciiFrameNode *node = mailbox->availableHead;
    while (node != NULL)
    {
        AsciiFrameNode *next = node->next;
        matrix_free(node->frame);
        free(node);
        node = next;
    }
    mailbox->availableHead = NULL;
    mailbox->availableTail = NULL;
    pthread_mutex_unlock(&mailbox->availableMutex);

    pthread_mutex_destroy(&mailbox->availableMutex);
    pthread_mutex_destroy(&mailbox->latestMutex);
    free(mailbox);
}

bool vcatAsciiFrameMailboxAcquireLatest(AsciiFrameMailbox *mailbox, matrix_t *outFrame)
{
    if (mailbox == NULL || outFrame == NULL)
    {
        return false;
    }

    bool hasFrame = false;
    pthread_mutex_lock(&mailbox->latestMutex);
    if (mailbox->latest != NULL)
    {
        *outFrame = mailbox->latest;
        mailbox->latest = NULL;
        hasFrame = true;
    }
    pthread_mutex_unlock(&mailbox->latestMutex);
    return hasFrame;
}

void vcatAsciiFrameMailboxRelease(AsciiFrameMailbox *mailbox, matrix_t frame)
{
    if (mailbox == NULL)
    {
        matrix_free(frame);
        return;
    }

    AsciiFrameNode *node = malloc(sizeof(*node));
    if (node == NULL)
    {
        // Nothing can be done about an allocation failure here; freeing the
        // frame loses it but keeps the pool from claiming it exists.
        matrix_free(frame);
        return;
    }
    node->frame = frame;
    node->next = NULL;

    pthread_mutex_lock(&mailbox->availableMutex);
    if (mailbox->availableTail == NULL)
    {
        mailbox->availableHead = node;
    }
    else
    {
        mailbox->availableTail->next = node;
    }
    mailbox->availableTail = node;
    pthread_mutex_unlock(&mailbox->availableMutex);
}

bool vcatAsciiFrameMailboxAcquireFree(AsciiFrameMailbox *mailbox, matrix_t *outFrame)
{
    if (mailbox == NULL || outFrame == NULL)
    {
        return false;
    }

    bool hasFrame = false;
    pthread_mutex_lock(&mailbox->availableMutex);
    AsciiFrameNode *node = mailbox->availableHead;
    if (node != NULL)
    {
        mailbox->availableHead = node->next;
        if (mailbox->availableHead == NULL)
        {
            mailbox->availableTail = NULL;
        }
        *outFrame = node->frame;
        free(node);
        hasFrame = true;
    }
    pthread_mutex_unlock(&mailbox->availableMutex);
    return hasFrame;
}

void vcatAsciiFrameMailboxPublish(AsciiFrameMailbox *mailbox, matrix_t frame)
{
    if (mailbox == NULL)
    {
        matrix_free(frame);
        return;
    }

    // Only the newest frame is kept: a consumer that falls behind gets the latest
    // data on its next acquire instead of draining a growing backlog of stale frames.
    matrix_t displaced = NULL;
    pthread_mutex_lock(&mailbox->latestMutex);
    displaced = mailbox->latest;
    mailbox->latest = frame;
    pthread_mutex_unlock(&mailbox->latestMutex);
    if (displaced != NULL)
    {
        vcatAsciiFrameMailboxRelease(mailbox, displaced); // Lock released first: keeps lock ordering one-way.
    }
}
