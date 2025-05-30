#ifndef BOARD_H
#define BOARD_H

/** Start address of the flash itself */
#define BOARD_FLASH_ADDR_ZERO 0x08000000UL

/** Start address of application space in flash */
#define BOARD_FLASH_APP_START 0x08008000UL

/** End address of application space (address of last byte) */
#define BOARD_FLASH_APP_END 0x0802FFFBUL

/** Size of the application space in flash (in words) */
#define BOARD_FLASH_APP_SIZE ((uint32_t)(((BOARD_FLASH_APP_END - BOARD_FLASH_APP_START) + 3UL) / 4UL))

/** Size of the total available flash */
#define BOARD_FLASH_SIZE 0x00030000UL

/** Start address of the application descriptor structure in flash */
#define BOARD_FLASH_APP_DESCRIPTOR 0x0802FF00UL

#define BOARD_UF2_FAMILY_ID 0x202E3A91 /*!< ST STM32L0xx */
#define USB_VID           0x16D0
#define USB_PID           0x1198
#define USB_MANUFACTURER  "Dektronics"
#define USB_PRODUCT       "Printalyzer UV/VIS Densitometer"
#define UF2_PRODUCT_NAME  USB_MANUFACTURER " " USB_PRODUCT
#define UF2_BOARD_ID      "STM32L072KZ-DPD105-revD"
#define UF2_VOLUME_LABEL  "UVDENSBOOT"
#define UF2_INDEX_URL     "https://www.dektronics.com/printalyzer-uvvis-dens"

#endif /* BOARD_H */
