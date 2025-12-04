/*
 * @Descripttion: 
 * @version: 
 * @Author: zhouhao
 * @Date: 2024-05-22 08:59:48
 * @LastEditors: zhouhao
 * @LastEditTime: 2024-07-11 15:23:13
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "ql_api_osi.h"
#include "ql_log.h"
#include "ql_pin_cfg.h"

#include "ql_fs.h"
//#include "audio_demo.h"
#include "ltweb_audio.h"
//#include "ltaudio.h"

#define LT_WEB_AUDIO_LOG_LEVEL	                QL_LOG_LEVEL_INFO
#define LT_WEB_AUDIO_LOG(msg, ...)			    QL_LOG(LT_WEB_AUDIO_LOG_LEVEL, "lt_webaudio", msg, ##__VA_ARGS__)
#define LT_WEB_AUDIO_LOG_PUSH(msg, ...)		    QL_LOG_PUSH("lt_webaudio", msg, ##__VA_ARGS__)


void generateRandomRadioId(char *radioId) {
    //srand(time(NULL));
    for (int i = 0; i < 30; i++) {
        radioId[i] = '0' + rand() % 10;
    }
    radioId[30] = '\0';
}



// Function to initialize AudioUrlManager
void AudioUrlManager_init(AudioUrlManager *manager) {
    manager->head = NULL;
    manager->tail = NULL;
    manager->size = 0;
    manager->currentIndex = 0;
}




// Function to add AudioUrl to AudioUrlManager
void AudioUrlManager_add(AudioUrlManager *manager,char *audioUrl) {
    // Allocate memory for the AudioUrlItem
    AudioUrlItem *new_item = (AudioUrlItem*)malloc(sizeof(AudioUrlItem));
    if (new_item == NULL) {
        LT_WEB_AUDIO_LOG("Failed to allocate memory\n");
        return ;
    }

    // Generate random radioId
    generateRandomRadioId(new_item->radioId);

    // Allocate memory for the AudioUrl and copy it
    new_item->audioUrl = (char*)malloc((strlen(audioUrl) + 1) * sizeof(char));
    if (new_item->audioUrl == NULL) {
        LT_WEB_AUDIO_LOG("Failed to allocate memory\n");
        free(new_item);
        return ;
    }
    strcpy(new_item->audioUrl, audioUrl);

    // Add the new item to the end of the list
    new_item->prev = manager->tail;
    new_item->next = NULL;
    if (manager->head == NULL) {
        manager->head = new_item;
    } else {
        manager->tail->next = new_item;
    }
    manager->tail = new_item;

    manager->size++;
	//s_ltau_sta.play_lst_cnt[1] ++;
}

// Function to get current AudioUrl from AudioUrlManager
 AudioUrlItem* AudioUrlManager_getCurrentAudioUrl(AudioUrlManager *manager) {
    if (manager->head == NULL) {
        return NULL;
    }

    // Find the current item
    AudioUrlItem *current = manager->head;
    for (size_t i = 0; i < manager->currentIndex; i++) {
        current = current->next;
    }

    return current;
}

// Function to move to next AudioUrl in AudioUrlManager
int AudioUrlManager_next(AudioUrlManager *manager) {
    if (manager->head == NULL) {
        return -1;
    }
 
    manager->currentIndex = (manager->currentIndex + 1) % manager->size;
     LT_WEB_AUDIO_LOG(" manager->currentIndex ==%d manager->size ==%d\n", manager->currentIndex,manager->size);
	// s_ltau_sta.play_lst_idx[1] = manager->currentIndex ;
   // manager->currentIndex = (manager->currentIndex + 1) % manager->size;
    return 0;
}

// Function to free memory allocated for AudioUrlManager
void AudioUrlManager_free(AudioUrlManager *manager) {
    AudioUrlItem *current = manager->head;
    while (current != NULL) {
        AudioUrlItem *next = current->next;
        free(current->audioUrl);
        free(current);
        current = next;
    }
    AudioUrlManager_init(manager);
}
// size_t audiourl_get_currentIndex()
// {
//     if(manager.size == 0)
//         return 0;
//     else
//         return manager.currentIndex +1;

// }

int AudioUrlManager_cur_audioname(AudioUrlManager *manager,char *data)
{
    AudioUrlItem *current = AudioUrlManager_getCurrentAudioUrl(manager);
    if(NULL != current)
    {
       strncpy(data,current->audioUrl,strlen(current->audioUrl)+1); 
       return  strlen(current->audioUrl);

    }
    return -1;

}
int AudioUrlManager_cur_radio_id(AudioUrlManager *manager,char *data)
{
    AudioUrlItem *current = AudioUrlManager_getCurrentAudioUrl(manager);
    if(NULL != current)
    {
       memcpy(data,current->radioId,sizeof(current->radioId)); 
       return  0;

    }
    return -1;

}
int audio_url_init(lt_audiourl_player_t * audiourl_player)
{

    audiourl_player->manager_init = AudioUrlManager_init;
    audiourl_player->audiourlitme_add = AudioUrlManager_add;
    audiourl_player->getCurrentAudioUrl = AudioUrlManager_getCurrentAudioUrl;
    audiourl_player->AudioUrlManager_free = AudioUrlManager_free;
    audiourl_player->AudioUrlManager_next = AudioUrlManager_next;

    LT_WEB_AUDIO_LOG("audio_url_init\n");
    if( audiourl_player->manager_init)
    {
            audiourl_player->manager_init(&(audiourl_player->manager)); 
    }
    
    return 0;
}


