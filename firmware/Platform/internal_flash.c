#include "board.h"
#include "main.h"
#include "lab3_diagnostic.h"
#include <string.h>
#define POINT_FLASH_BASE 0x08007000u
#ifdef LAB3_DIAGNOSTIC
volatile FlashDiagnostic flash_diagnostic;
static void flash_begin(uint32_t operation, uint32_t offset) {
    memset((void *)&flash_diagnostic,0,sizeof(flash_diagnostic));
    flash_diagnostic.operation=operation; flash_diagnostic.offset=offset;
    flash_diagnostic.hal_result=UINT32_MAX;
    flash_diagnostic.page_error=UINT32_MAX;
    flash_diagnostic.first_bad_offset=UINT32_MAX;
    flash_diagnostic.sr_before=FLASH->SR; flash_diagnostic.cr_before=FLASH->CR;
}
static void flash_end(HAL_StatusTypeDef status, uint32_t page_error) {
    flash_diagnostic.hal_result=status; flash_diagnostic.page_error=page_error;
    flash_diagnostic.hal_error=HAL_FLASH_GetError();
    flash_diagnostic.sr_after=FLASH->SR; flash_diagnostic.cr_after=FLASH->CR;
    flash_diagnostic.lock_result=HAL_FLASH_Lock();
}
#endif
static void read_bytes(void *context, unsigned offset, void *out, unsigned n) {
    (void)context;
    const volatile uint8_t *source=(const volatile uint8_t *)(POINT_FLASH_BASE+offset);
    uint8_t *target=out;
    for(unsigned i=0;i<n;++i) target[i]=source[i];
}
static bool erase_page(void *context, unsigned page) {
    (void)context;
    if (page>1) return false;
#ifdef LAB3_DIAGNOSTIC
    flash_begin(1,page*2048u);
    flash_diagnostic.unlock_result=HAL_FLASH_Unlock();
    if(flash_diagnostic.unlock_result!=HAL_OK) { flash_diagnostic.complete=1; return false; }
#else
    if(HAL_FLASH_Unlock()!=HAL_OK) return false;
#endif
    FLASH_EraseInitTypeDef erase={0}; uint32_t error=0;
    erase.TypeErase=FLASH_TYPEERASE_PAGES;
    erase.Page=14u+page; erase.NbPages=1;
    HAL_GPIO_WritePin(GPIOA,GPIO_PIN_6,GPIO_PIN_SET);
    HAL_StatusTypeDef status=HAL_FLASHEx_Erase(&erase,&error);
    bool ok=status==HAL_OK;
    HAL_GPIO_WritePin(GPIOA,GPIO_PIN_6,GPIO_PIN_RESET);
#ifdef LAB3_DIAGNOSTIC
    flash_end(status,error);
    const volatile uint32_t *words=(const volatile uint32_t *)(POINT_FLASH_BASE+page*2048u);
    flash_diagnostic.first_word=words[0]; flash_diagnostic.last_word=words[511];
    for(unsigned i=0;i<512;++i) {
        uint32_t word=words[i]; ++flash_diagnostic.checked_words;
        if(word!=UINT32_MAX) {
            if(!flash_diagnostic.non_ff_words) {
                flash_diagnostic.first_bad_offset=i*4u; flash_diagnostic.first_bad_word=word;
            }
            ++flash_diagnostic.non_ff_words;
        }
    }
    flash_diagnostic.complete=1;
    return ok && !flash_diagnostic.non_ff_words;
#else
    (void)HAL_FLASH_Lock();
    if (ok) {
        const volatile uint32_t *p=(const volatile uint32_t *)(POINT_FLASH_BASE+page*2048);
        for (unsigned i=0;i<512;++i) if(p[i]!=UINT32_MAX) return false;
    }
    return ok;
#endif
}
static bool program_word(void *context, unsigned offset, const uint8_t bytes[8]) {
    (void)context;
    if(offset>4088 || offset%8) return false;
#ifdef LAB3_DIAGNOSTIC
    flash_begin(2,offset);
    flash_diagnostic.unlock_result=HAL_FLASH_Unlock();
    if(flash_diagnostic.unlock_result!=HAL_OK) { flash_diagnostic.complete=1; return false; }
#else
    if(HAL_FLASH_Unlock()!=HAL_OK) return false;
#endif
    uint64_t word; memcpy(&word,bytes,8);
    HAL_GPIO_WritePin(GPIOA,GPIO_PIN_6,GPIO_PIN_SET);
    HAL_StatusTypeDef status=HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD,POINT_FLASH_BASE+offset,word);
    bool ok=status==HAL_OK;
    HAL_GPIO_WritePin(GPIOA,GPIO_PIN_6,GPIO_PIN_RESET);
#ifdef LAB3_DIAGNOSTIC
    flash_end(status,UINT32_MAX);
    flash_diagnostic.complete=1;
#else
    (void)HAL_FLASH_Lock();
#endif
    return ok;
}
PointFlash board_point_flash(void) { return (PointFlash){NULL,read_bytes,erase_page,program_word}; }
