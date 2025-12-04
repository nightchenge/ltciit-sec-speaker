/*
 * @Descripttion: 
 * @version: 
 * @Author: zhouhao
 * @Date: 2024-05-28 09:27:36
 * @LastEditors: zhouhao
 * @LastEditTime: 2024-05-29 10:45:22
 */
/*
 * @Descripttion: 
 * @version: 
 * @Author: zhouhao
 * @Date: 2024-05-22 09:08:25
 * @LastEditors: zhouhao
 * @LastEditTime: 2024-05-22 09:08:49
 */
#ifndef _LTWEB_AUDIO_H_
#define _LTWEB_AUDIO_H_

typedef struct AudioUrlItem {
    char *audioUrl;
    char radioId[31];
    struct AudioUrlItem *prev;
    struct AudioUrlItem *next;
} AudioUrlItem;

// Structure to manage AudioUrl list
typedef struct {
    AudioUrlItem *head;
    AudioUrlItem *tail;
    size_t size;
    size_t currentIndex;
} AudioUrlManager;


typedef struct lt_audiourl_player{
    AudioUrlManager manager;
    void (*manager_init)(AudioUrlManager *manager);
    void (*audiourlitme_add)(AudioUrlManager *manager, char *audioUrl);
    AudioUrlItem* (*getCurrentAudioUrl)( AudioUrlManager *manager);
    int (*AudioUrlManager_next)(AudioUrlManager *manager);
    void (*AudioUrlManager_free)(AudioUrlManager *manager);
   // void (*audiourl_play)(int type,AudioUrlManager *manager);
} lt_audiourl_player_t;
void generateRandomRadioId(char *radioId);
int AudioUrlManager_cur_audioname(AudioUrlManager *manager,char *data);
int AudioUrlManager_cur_radio_id(AudioUrlManager *manager,char *data);
int audio_url_init(lt_audiourl_player_t * audiourl_player);


#endif // _LTWEB_AUDIO_H_