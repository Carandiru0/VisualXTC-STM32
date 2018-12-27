   AREA    Font_BinFile1_Section, DATA, READONLY
        
		
	EXPORT  Gothica64x128_8x18

; Includes the binary file ***.8bit from the current source folder
Gothica64x128_8x18
        INCBIN  gothica64x128_8x18.8bit
Gothica64x128_8x18_End


	EXPORT  LadyRadical64x128_9x14

; Includes the binary file ***.8bit from the current source folder
LadyRadical64x128_9x14
        INCBIN  LadyRadical64x128_9x14.8bit
LadyRadical64x128_9x14_End
		
; ************************************************************** ;

;	AREA    Noise_BinFile1_Section, DATA, READONLY
        
		
;	EXPORT  BlueNoise_x256

; Includes the binary file ***.8bit from the current source folder
;BlueNoise_x256
;	INCBIN  ..\Data\bluenoise256.data
;BlueNoise_x256_End
; define a constant which contains the size of the image above
;BlueNoise_x256_size
;        DCDU    BlueNoise_x256_End - BlueNoise_x256
			
;        EXPORT  BlueNoise_x256_size
			
; ************************************************************** ;

	AREA    AO_Ground_Section, DATA, READONLY
        
		
	EXPORT  _flash_top_ao_bothsides

; Includes the binary file ***.8bit from the current source folder
_flash_top_ao_bothsides
	INCBIN  ..\Data\AO\top_ao_bothsides.data
_flash_top_ao_bothsides_End
; define a constant which contains the size of the image above
top_ao_bothsides_size
        DCDU    _flash_top_ao_bothsides_End - _flash_top_ao_bothsides
			
        EXPORT  top_ao_bothsides_size
			
; ************************************************************** ;

	EXPORT  _flash_top_ao_oneside

; Includes the binary file ***.8bit from the current source folder
_flash_top_ao_oneside
	INCBIN  ..\Data\AO\top_ao_oneside.data
_flash_top_ao_oneside_End
; define a constant which contains the size of the image above
top_ao_oneside_size
        DCDU    _flash_top_ao_oneside_End - _flash_top_ao_oneside
			
        EXPORT  top_ao_oneside_size

; ************************************************************** ;

	EXPORT  _flash_side_7_both_ao

; Includes the binary file ***.8bit from the current source folder
_flash_side_7_both_ao
	INCBIN  ..\Data\AO\side_7_both_ao.data
_flash_side_7_both_ao_End
; define a constant which contains the size of the image above
side_7_both_ao_size
        DCDU    _flash_side_7_both_ao_End - _flash_side_7_both_ao
			
        EXPORT  side_7_both_ao_size
			
; ************************************************************** ;

	EXPORT  _flash_side_7_botom_ao

; Includes the binary file ***.8bit from the current source folder
_flash_side_7_botom_ao
	INCBIN  ..\Data\AO\side_7_botom_ao.data
_flash_side_7_botom_ao_End
; define a constant which contains the size of the image above
side_7_botom_ao_size
        DCDU    _flash_side_7_botom_ao_End - _flash_side_7_botom_ao
			
        EXPORT  side_7_botom_ao_size

; ************************************************************** ;

	AREA    VOX_Section, DATA, READONLY
        
		
	EXPORT  _vox_sr71
_vox_sr71
	INCBIN  ..\Data\VOX\sr71.vox
	
	EXPORT  _vox_f2a
_vox_f2a
	INCBIN  ..\Data\VOX\f2a.vox
		
	EXPORT  _vox_sidewinder
_vox_sidewinder
	INCBIN  ..\Data\VOX\sidewinder.vox
; ************************************************************** ;

















	END
		