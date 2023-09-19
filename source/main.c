//#include <stdarg.h>        //< va_list, va_start, va_arg
//#include <string.h>        //< memcpy, memset, strlen
#include <stdlib.h>        //< Used for itoa, srand and exit
#include <stdio.h>         //< printf and fopen
#include <conio.h>         //< cgetc
#include <cx16.h>          //< videomode, etc.

#include <core_x16.h>      //< some X16 keyboard and direct video support routines

#define TRUE  1
#define FALSE 0

// Display Mode
#define DM_80COL 0
#define DM_40COL 1

// Display Mode Height
#define DH_60ROW 0
#define DH_30ROW 1
#define DH_15ROW 2	

//CX16_clock_type cx16_clock;  //< don't think we'll need this clock...

unsigned char force_full_refresh;  // Set to TRUE when we need to refresh the entire main display
	
#define MAX_PATH_AND_FILENAME_LEN 80
typedef struct 
{
	unsigned char display_index;        // index of the current display mode
	unsigned char display_mode;         // mode 0 = 80col, 1 = 40 col
	unsigned char display_mode_height;  // this is like a sub-mode, for mode 0: 0=60,1=30,  for mode 1: 0=30,1=15
	
	unsigned char curr_x;  // cursor location x (across col)
	unsigned char curr_y;  // cursor location y (vertical row)
	unsigned long curr_byte_idx;  // byte index into the current SCREEN (not the file! use offset_min + curr_byte_idx to get the file index)
	
	unsigned char curr_height;
	unsigned char curr_width;
	
	unsigned char original_text_fg;
	unsigned char original_text_bg;
	signed char original_video_mode;
	
	char input_filename[MAX_PATH_AND_FILENAME_LEN];
	
  unsigned char byte_spacing;      // amount of spacing between each hex byte value
	unsigned char decode_offset;     // where to start the decoded version of the hex values
	unsigned char divider1_offset;   // first  divider line offset column
	unsigned char divider2_offset;   // second divider line offset column
	unsigned char divider3_offset;   // third  divider line offset column
	unsigned char row_top;
	unsigned char row_bottom;	
	unsigned char byte_per_row;      // typically 8 or 16
	unsigned char max_column;
	
	unsigned char col_key;
	unsigned char col_key_modifier;
	unsigned char col_bytes_read;
	
	unsigned long bytes_read;
	
	unsigned long offset_min;  // offset into the file to start showing (beginning)
	unsigned long offset_max;  // offset into the file to stop showing (ending)
	
	unsigned char buffer_modified;
	
} Main_program_state;
Main_program_state main_program_state;

void print_xy_ulong_as_decimal(unsigned char x, unsigned char y, unsigned long value, char color, unsigned char left_pad)
{
	unsigned char b_index;  // buffer index
	unsigned char str_len;
	char buffer[20];
	char ch;
	unsigned char i;
	unsigned char target_x;
	
	ltoa(value, buffer, 10);  // buffer will be null terminated \0    base-10 (10 == decimal)
	
	if (left_pad > 0)
	{
		str_len = 0;
		while (TRUE)
		{
			ch = buffer[str_len];
			if (buffer[str_len] == '\0')
			{
				break;  // found the null terminator, break out of the loop
			}
			++str_len;
		}
		
		// str_len is now the LENGTH of the string
		if (left_pad >= str_len)
		{
		  ch = (left_pad - str_len);		  
		  for (i = 0; i < ch; ++i)  // write the remaining left pads
		  {
				CX16_WRITE_XY(x, y, CX16_M2_CHAR_SPACE);
				CX16_COLOR_XY(x, y, color);		
				++x;
		  }
			goto full_output;
		}
		else
		{
			// the length of the "value" is longer than the pad, just output the full value
			goto full_output;
		}
	}
	else
	{
full_output:		
		b_index = 0;
		while (TRUE)
		{
			ch = buffer[b_index];

			target_x = x+b_index;
			CX16_WRITE_XY(target_x, y, ch);  //ascii_to_x16[i2]);  
			// because ASCII and X16 intersect on '0' to '9', no extra lookup is needed for numbers
			
			CX16_COLOR_XY(target_x, y, color);		
			
			++b_index;
			if (buffer[b_index] == '\0')
			{
				break;  // found the null terminator, break out of the loop
			}
			
			if (b_index > 18)  // safety margin
			{
				break;
			}
		}	
	}
}

unsigned char hex2value(char* hex)
{
	unsigned char val = 0;
	while (*hex) 
	{		
		// get current character then increment
		// 48 == 0, 57 == 9, 65 == 'A', 70 == 'F'
		unsigned char byte = *hex++; 
		// transform hex character to the 4bit equivalent number, using the ascii table indexes
		if (byte >= 48 && byte <= 57) byte = byte - 48;
		//else if (byte >= 'a' && byte <='f') byte = byte - 'a' + 10;  we've forced uppercase earlier
		else if (byte >= 65 && byte <= 70) byte = byte - 65 + 10;    
		// shift 4 to make space for new digit, and add the 4 bits of the new digit 
		val = (val << 4) | (byte & 0xF);
	}
	return val;
}

void print_xy_ulong_as_hex(unsigned char x, unsigned char y, unsigned long value, char color, unsigned char left_pad)
{
	unsigned char b_index;  // buffer index
	unsigned char str_len;
	char buffer[20];
	char ch;
	unsigned char i;
	unsigned char target_x;
	
	ltoa(value, buffer, 16);  // buffer will be null terminated \0    base-10 (10 == decimal)
	
	if (left_pad > 0)
	{
		str_len = 0;
		while (TRUE)
		{
			ch = buffer[str_len];
			if (buffer[str_len] == '\0')
			{
				break;  // found the null terminator, break out of the loop
			}
			++str_len;
		}
		
		// str_len is now the LENGTH of the string
		if (left_pad >= str_len)
		{
		  ch = (left_pad - str_len);		  
		  for (i = 0; i < ch; ++i)  // write the remaining left pads
		  {
				CX16_WRITE_XY(x, y, CX16_M2_CHAR_0);
				CX16_COLOR_XY(x, y, color);		
				++x;
		  }
			goto full_output;
		}
		else
		{
			// the length of the "value" is longer than the pad, just output the full value
			goto full_output;
		}
	}
	else
	{
full_output:		
		b_index = 0;
		while (TRUE)
		{
			ch = buffer[b_index];

			target_x = x+b_index;
			CX16_WRITE_XY(target_x, y, ch);  //ascii_to_x16[i2]);  
			// because ASCII and X16 intersect on '0' to '9', no extra lookup is needed for numbers
			
			CX16_COLOR_XY(target_x, y, color);		
			
			++b_index;
			if (buffer[b_index] == '\0')
			{
				break;  // found the null terminator, break out of the loop
			}
			
			if (b_index > 18)  // safety margin
			{
				break;
			}
		}	
	}
}

#define FIRST_COLUMN 11
#define FIRST_ROW 2
#define DM_80COL 0
#define DM_40COL 1

#define DH_60ROW 0
#define DH_30ROW 1
#define DH_15ROW 2	

#define DS_IDX_BYTE_SPACING    0
#define DS_IDX_DECODE_OFFSET   1
#define DS_IDX_DIV1_OFFSET     2
#define DS_IDX_DIV2_OFFSET     3
#define DS_IDX_DIV3_OFFSET     4
#define DS_IDX_ROW_TOP         5
#define DS_IDX_ROW_BOTTOM      6
#define DS_IDX_WIDTH           7
#define DS_IDX_HEIGHT          8
#define DS_IDX_BYTE_PER_ROW    9
#define DS_IDX_COL_KEY         10
#define DS_IDX_COL_KEY_MOD     11
#define DS_IDX_COL_BYTES_READ  12
/*
  These arrays define the column offset of various content of the main display.
	These positions change based on the text-mode screen resolution.
*/
unsigned char display_state[][] =
{
	//   dec                                 col col col
	//sp ofs d1  d2  d3  top bot wi  hi  bpr key mod byt
	// -------------------------------- MODE 1
	{ 3, 59, 10, 34, 58,  2, 58, 80, 60, 16, 40, 44, 72},  // 80x60
  { 3, 59, 10, 34, 58,  2, 28, 80, 30, 16, 40, 44, 72},  // 80x30
	// -------------------------------- MODE 2
	//{ 2, 28, 10, 27, 36,  2, 58, 40, 60,  8, 20, 24, 32},  // 40x60  (was hard to read, removed support for this mode)
	{ 2, 28, 10, 27, 36,  2, 28, 40, 30,  8, 20, 24, 32},  // 40x30
	{ 2, 28, 10, 27, 36,  2, 13, 40, 15,  8, 20, 24, 32}   // 40x15
};

void reset_to_top()
{
  main_program_state.offset_min = 0;
  main_program_state.offset_max = ((main_program_state.row_bottom - main_program_state.row_top)+1) * main_program_state.byte_per_row;
}

void update_bytes_read_display()
{
	//    65535   64KB
	//   128345  128KB
	//  2123445    2MB
	//sprintf(str, "    /%7lu", main_program_state.bytes_read);
	//cputsxy(main_program_state.col_bytes_read, 0, str);
	print_xy_ulong_as_decimal(main_program_state.col_bytes_read, 0, main_program_state.bytes_read, CX16_BG_BLUE | CX16_FG_WHITE, 8);	
}

void draw_main_screen_overlay(char complete)
{
/*
MODE 1 (80col) x60x30
          1         2         3         4         5         6         7         8
01234567890123456789012345678901234567890123456789012345678901234567890123456789
          |                       |                       |12345678        
Offset (h) 00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F Decoded             <
AAAA BBBB  xx xx xx xx

MODE 2 (40col) x60x30x15
          1         2         3         4
0123456789012345678901234567890123456789
                                      
Offset (h) 0001020304050607 Decoded    <
AAAA BBBB  xx xx xx xx

*/
	
	unsigned char idx;
	unsigned char idx_value;
	char str[128];
	
	if (complete == TRUE)
	{
	  ENABLE_SCREEN_MODE_1;
	}
	  
	textcolor(COLOR_WHITE);
	bgcolor(COLOR_BLUE);
	
	//CLRSCR;  // will get cleared when videomode is set below	
		
	if (complete == TRUE)
	{
	  ENABLE_TEXT_MODE_2;  			
	
	  main_program_state.curr_x = FIRST_COLUMN;
	  main_program_state.curr_y = 2;
	}

try_again:
	switch (main_program_state.display_index)  
	{
  case 0:  // 80x60
	  {
			main_program_state.display_mode = DM_80COL;
			main_program_state.display_mode_height = DH_60ROW;
	    videomode( VIDEOMODE_80x60 );
	  }
	  break;
	case 1:  // 80x30
	  {
			main_program_state.display_mode = DM_80COL;
			main_program_state.display_mode_height = DH_30ROW;
	    videomode( VIDEOMODE_80x30 );
	  }
	  break;
	//case 2:  // 40x60
	//  {
	//		main_program_state.display_mode = DM_40COL;
	//		main_program_state.display_mode_height = DH_60ROW;
	//    videomode( VIDEOMODE_40x60 );
	//  }
	//  break;
	case 2:  // 40x30
	  {
			main_program_state.display_mode = DM_40COL;
			main_program_state.display_mode_height = DH_30ROW;
	    videomode( VIDEOMODE_40x30 );
	  }
	  break;
	case 3:  // 40x15
	  {
			main_program_state.display_mode = DM_40COL;
			main_program_state.display_mode_height = DH_15ROW;
	    videomode( VIDEOMODE_40x15 );
	  }
	  break;
	default:  // revert back to display index 0
	  {
	    main_program_state.display_index = 0;
			goto try_again;
		}
		break;
	}	
	
	// Store down the display-state configuration...
	main_program_state.byte_spacing = display_state[main_program_state.display_index][DS_IDX_BYTE_SPACING];
	main_program_state.decode_offset = display_state[main_program_state.display_index][DS_IDX_DECODE_OFFSET];
	main_program_state.divider1_offset = display_state[main_program_state.display_index][DS_IDX_DIV1_OFFSET];
	main_program_state.divider2_offset = display_state[main_program_state.display_index][DS_IDX_DIV2_OFFSET];
	main_program_state.divider3_offset = display_state[main_program_state.display_index][DS_IDX_DIV3_OFFSET];
	main_program_state.row_top = display_state[main_program_state.display_index][DS_IDX_ROW_TOP];
	main_program_state.row_bottom = display_state[main_program_state.display_index][DS_IDX_ROW_BOTTOM];		
	main_program_state.curr_width = display_state[main_program_state.display_index][DS_IDX_WIDTH] - 1;
	main_program_state.curr_height = display_state[main_program_state.display_index][DS_IDX_HEIGHT] - 1;
	main_program_state.byte_per_row = display_state[main_program_state.display_index][DS_IDX_BYTE_PER_ROW];
	main_program_state.max_column = FIRST_COLUMN + (main_program_state.byte_per_row-1)*main_program_state.byte_spacing;
	main_program_state.col_key = display_state[main_program_state.display_index][DS_IDX_COL_KEY];
	main_program_state.col_key_modifier = display_state[main_program_state.display_index][DS_IDX_COL_KEY_MOD];
	main_program_state.col_bytes_read = display_state[main_program_state.display_index][DS_IDX_COL_BYTES_READ];
	
	// ROW 0 is for filename and status...
	sprintf(str, "FILE: %s", main_program_state.input_filename);
	cputsxy(0, 0, str);
	
	update_bytes_read_display();
	
	// ROW 1 (column labels)
	cputsxy(0, 1, "Offset (h)");
	cputsxy(main_program_state.decode_offset, 1, "petscii");	
	idx = FIRST_COLUMN;
	idx_value = 0;	
	while (idx_value < main_program_state.byte_per_row)
	{
		if (idx_value % 2 == 0)
		{
			textcolor(COLOR_WHITE);	
		}
		else
		{
			textcolor(COLOR_GRAY2);	
		}
		sprintf(str, "%02X", idx_value);
		cputsxy(idx, 1, str);		
		idx += main_program_state.byte_spacing;
		++idx_value;
	}
	
	// Draw the column divider bars
	for (idx = main_program_state.row_top; idx <= main_program_state.row_bottom; ++idx)
	{
		cputcxy(main_program_state.divider1_offset, idx, '|');
		cputcxy(main_program_state.divider2_offset, idx, '|');
		cputcxy(main_program_state.divider3_offset, idx, '|');
	}

  if (complete == TRUE)
	{
    reset_to_top();	  
	
	  main_program_state.curr_byte_idx = 0;
	}
	
	force_full_refresh = TRUE;
}

void show_help()
{
	char ch;
	
	CLRSCR;
	
	textcolor(COLOR_WHITE);
	
//        1234567890123456789012345678901234567890
  printf("\nX16HXD V1.3 (BY VOIDSTAR)\n----------------------------\n");
	printf("ARROW KEYS MOVE THE CURSOR\n");
	printf("0-F   MODIFY CURRENT BYTE (HEX)\n");
	printf("[ ]   MOVE ONE ROW UP/DOWN\n");
	printf("PGUP  MOVE UP HALF SCREEN\n");
	printf("PGDN  MOVE DOWN HALF SCREEN\n");
	printf("HOME  MOVE TO TOP OF FILE\n");
	printf("INS   INSERT EMPTY BYTE AT CURSOR\n");
	printf("DEL   REMOVE BYTE AT CURSOR\n");
	printf("G     GOTO OFFSET (USE DECIMAL)\n");
	printf("V     CHANGE VIDEO MODE\n");
	printf("W     WRITE CHANGES BACK TO FILE\n");	
	printf("X|ESC EXIT BACK TO BASIC\n");		
	printf("\nPRESS ANY KEY");

	while (TRUE)
	{
	  if (kbhit())
		{
			ch = cgetc();  // consume the key that wsa hit...
			break;
		}
	}
	
}

/*
long determine_file_size(char *filename) 
{
	long size;
  FILE *fp = fopen(filename, "r");

  if (fp==0)
    return -1;

  if (fseek(fp, 0, SEEK_END) < 0) 
	{
    fclose(fp);
    return -1;
  }

  //size = ftell(fp);  // not implemented for cx16.lib
  
  fclose(fp);
  return size;
}
*/

/*
We only show 6 bytes at a time, so 60 rows is 16*60 = 960 byte for a single screen at a time.
We'll want to buffer up multiple screens of data at a time (to help with search and other features).
The code size of the PRG will impact the available memory for this.
*/
//#define MAX_BUFFER_SIZE 8192
#define MAX_BUFFER_SIZE 20496
unsigned char* hex_data_buffer;

void main(void)
{	
	unsigned char ch;  // non-zero initial value is used to trigger a key check and force display update on startup
	unsigned char ch_modifier;
	int input_ch;
  int int_value;	
	unsigned long idx = 0;
	char goto_addr_str[10];
	unsigned long goto_addr;
	char hex_color;
	unsigned char idx_col;
	unsigned char idx_row;
	unsigned char idx_bytes_this_row;
	FILE* f = 0;		
	
	unsigned char edit_index = 0;  // 0==not editing, 1=slot 1, 2=slot 2
	char edit_str[3];
	
	edit_str[0] = '\0';
	edit_str[1] = '\0';
	edit_str[2] = '\0';
	
//	cx16_init_clock();  // clock not really needed for this program
		
	main_program_state.display_index = 1;		
	main_program_state.curr_byte_idx = 0;	
	
	int_value = 8192;
	while (TRUE)
	{
		hex_data_buffer = (unsigned char*)malloc(int_value);	
		if (hex_data_buffer == 0)
		{
			int_value -= 128;
			break;
		}
		else
		{
			free(hex_data_buffer);
			int_value += 128;
		}
	}
	
	hex_data_buffer = (unsigned char*)malloc(int_value);		
	if (hex_data_buffer == 0)
	{
		printf("insufficient memory for hex data buffer.");
		exit(-1);
	}
	
	// Go ahead and clear out the processing buffer
	for (input_ch = 0; input_ch < MAX_BUFFER_SIZE; ++input_ch)
	{
		hex_data_buffer[input_ch] = 0;
	}		
	
	printf("MAX FILESIZE [%d] BYTES\n", int_value);
	// TBD: file selection		
	printf("INPUT FILE: ");
	scanf("%s", &main_program_state.input_filename);	

  // OPEN AND READ THE FILE CONTENTS INTO RAM	
	f = fopen(main_program_state.input_filename, "rb");
	if (f == 0)
	{
		printf("FILE NOT FOUND.\n");
		printf("ASSUMING NEW FILE.\n");
		printf("HOW MANY BYTES? ");
		scanf("%u", &main_program_state.bytes_read);
		if (main_program_state.bytes_read > MAX_BUFFER_SIZE)  // restrict to max size
		{
			main_program_state.bytes_read = MAX_BUFFER_SIZE;
		}		
		// we don't exit, because this gives the chance for the user
		// to create a new file.
	}
	else
	{
		// Read the file into the available memory buffer
		idx = 0;
		while (TRUE)
		{
			input_ch = fgetc(f);
			
			if (feof(f))
			{
				break;
			}			

			hex_data_buffer[idx] = input_ch;			
			
			++idx;
			if (idx >= MAX_BUFFER_SIZE)
			{
				break;
			}			
		}
		fclose(f);
		main_program_state.bytes_read = idx;
	}	
			
	main_program_state.original_text_fg = textcolor(CX16_FG_WHITE);
	main_program_state.original_text_bg = bgcolor(CX16_BG_BLUE);	
	main_program_state.original_video_mode = videomode(VIDEOMODE_80x60);		
			
	draw_main_screen_overlay(TRUE);	
		
	goto update_hex_display;
	
	while (TRUE)
	{			
    // IF A KEYBOARD KEYPRESS WAS DETECTED, TAKE ACTION BASED ON THE KEY THAT WAS PRESSED...			
		if (kbhit())
		{
			ch = cgetc();
			ch_modifier = check_kbd_modifiers();

			if (
			 ((ch >= 'A') && (ch <= 'Z'))  // upper case
			)
			{				
				ch -= 128;  // go back to lower case version, but modified will show SHIFT
			}
			else if (
			 ((ch >= 'a') && (ch <= 'z'))  // lower case
			)
			{
				// no adjustment
				//ch += 128;
			}
			else if (
			 ((ch >= 48) && (ch <= 57))  // 0 through 9
      )
			{				
				//ch += 128;
			}			
			
      // DISPLAY LAST ch keycode and modifier
			{				
			
				CX16_WRITE_XY(main_program_state.col_key, main_program_state.curr_height, CX16_M2_CHAR_SPACE);  // clear the prior key
				CX16_WRITE(CX16_M2_CHAR_SPACE);
				CX16_WRITE(CX16_M2_CHAR_SPACE);				
        print_xy_ulong_as_decimal(main_program_state.col_key, main_program_state.curr_height, ch, CX16_BG_BLACK | CX16_FG_YELLOW, 0);
				
				CX16_WRITE_XY(main_program_state.col_key_modifier, main_program_state.curr_height, CX16_M2_CHAR_SPACE);  // clear the prior modifier key
				CX16_WRITE(CX16_M2_CHAR_SPACE);					
        print_xy_ulong_as_decimal(main_program_state.col_key_modifier, main_program_state.curr_height, ch_modifier, CX16_BG_BLACK | CX16_FG_YELLOW, 0);
			}

advance_cursor:			
			switch (ch)
			{
		  case 'v':
			  {					
					++main_program_state.display_index;
					draw_main_screen_overlay(TRUE);
				}
				break;
				
			case 'w':
			  {
					textcolor(COLOR_WHITE);
					f = fopen(main_program_state.input_filename, "wb");
					if (f == 0)
					{						
						cputsxy(3, main_program_state.curr_height, "ERROR W1");
						// BEEP
						printf("\a");  // BEEP "Error write filename.");						
					}
					else
					{
						for (input_ch = 0; input_ch < main_program_state.bytes_read; ++input_ch)
						{
							int_value = fputc(hex_data_buffer[input_ch], f);
							if (int_value == EOF)
							{
								cputsxy(3, main_program_state.curr_height, "ERROR W2");
								// BEEP
								printf("\a");  // BEEP "Error write filename.");														
							}
						}
						fclose(f);
						if (input_ch == main_program_state.bytes_read)  // entire file was written
					  {
							cputsxy(3, main_program_state.curr_height, "WRITE OK");
						  main_program_state.buffer_modified = FALSE;
						}
					}					
			  }
			  break;	
				
			case 48:  // 0
			case 49:  // 1
			case 50:  // 2
			case 51:  // 3
			case 52:  // 4
			case 53:  // 5
			case 54:  // 6
			case 55:  // 7
			case 56:  // 8
			case 57:  // 9
			case 65:  // A
			case 66:  // B
			case 67:  // C
			case 68:  // D
			case 69:  // E
			case 70:  // F
			  {
					if (edit_index == 0)
					{
						++edit_index;
						edit_str[0] = ch;
						main_program_state.buffer_modified = TRUE;
						
						// If modifying past the end of the current file, then extend the size of current file...
						if ((main_program_state.offset_min + main_program_state.curr_byte_idx) > main_program_state.bytes_read)
						{
							main_program_state.bytes_read = (main_program_state.offset_min + main_program_state.curr_byte_idx);	
							update_bytes_read_display();						  
  						force_full_refresh = TRUE;
						}
					}
					else if (edit_index == 1)
					{
						++edit_index;
						edit_str[1] = ch;
						ch = 29;  // RIGHT_ARROW
						goto advance_cursor;  // run the case again to process the RIGHT_ARROW movement
					}										
			  }
			  break;
				
			case 'h':  // H help
			  {
					show_help();
          draw_main_screen_overlay(FALSE);	
		     	goto update_hex_display;					
					break;  // not really necessary
			  }
				
		  case 'g':  // G goto
			  {
					if (edit_index > 0)
					{
						// do nothing
					}
					else
					{	
            textcolor(COLOR_WHITE);				
				    goto_addr = 0;
						cputsxy(0, main_program_state.curr_height, "GOTO (DEC):");												
						
						idx = 0;
						while (TRUE)
						{
							if (kbhit())
							{
								input_ch = cgetc();
								ch_modifier = check_kbd_modifiers();
								//printf("%02X", input_ch);
								if ((input_ch >= '0') && (input_ch <= '9') && (idx < 5))
								{
									goto_addr_str[idx] = input_ch;
									++idx;
									cputc(input_ch);
								}
								else if (
									(input_ch == 0x14)  // backspace
									&& (idx > 0)
								)
								{
									gotoxy(wherex()-1,wherey());								
									cputc(' ');
									gotoxy(wherex()-1,wherey());								
									--idx;
								}
								else if (input_ch == 0x0D)
								{
									goto_addr_str[idx] = '\0';
									break;
								}
							}
						}
						sscanf(goto_addr_str, "%lu", &goto_addr);						
						
						if (goto_addr > MAX_BUFFER_SIZE)
						{
							// don't go past the end of file
							goto_addr = MAX_BUFFER_SIZE;
						}
						
						// go back to initial conditions...
						main_program_state.offset_min = 0;
						main_program_state.offset_max = ((main_program_state.row_bottom - main_program_state.row_top)+1) * main_program_state.byte_per_row;
						
						main_program_state.curr_x = FIRST_COLUMN;
						main_program_state.curr_y = 2;
						main_program_state.curr_byte_idx = 0;

            // now search for the GOTO position...						
						while (TRUE)
						{							
					    if ((main_program_state.offset_min + main_program_state.curr_byte_idx) >= goto_addr)
							{								
								break;
							}						
							
							main_program_state.curr_x += main_program_state.byte_spacing;
							if (main_program_state.curr_x > main_program_state.max_column)
							{
								// go to the next row...
								main_program_state.curr_x = FIRST_COLUMN;  // reset x back to starting column
								++main_program_state.curr_y;
								
								if (main_program_state.curr_y >= main_program_state.row_bottom) 
								{
									main_program_state.curr_y = main_program_state.row_bottom-1;
									
									main_program_state.offset_min += main_program_state.byte_per_row;
					        main_program_state.offset_max += main_program_state.byte_per_row;
									
									main_program_state.curr_byte_idx -= main_program_state.byte_per_row;
								}
							}					    
							
							++main_program_state.curr_byte_idx;
						}
						cputsxy(0, main_program_state.curr_height, "                   ");
						force_full_refresh = TRUE;
					}					
					break;
			  }
				
			case 27:  // ESCAPE
			case 'x':  // X exit
			  {
  				if (edit_index > 0)
					{
						// cancel the edit
						edit_index = 0;
						force_full_refresh = TRUE;
					}
					else
					{
						if (main_program_state.buffer_modified)
						{
							// Are you SURE!?
							printf("\a");
							textcolor(COLOR_WHITE);
							cputsxy(0, main_program_state.curr_height, "NOT SAVED. EXIT? Y/N");
							while (TRUE)
							{
								if (kbhit())
								{
									ch = cgetc();  // consume the key that wsa hit...
									if ((ch == 'y') || (ch == 'Y'))
									{
										break;
									}
									else if ((ch == 'n') || (ch == 'N'))
									{
										cputsxy(0, main_program_state.curr_height, "                    ");
										goto cancel_exit;
									}
								}
							}						
						}
						videomode(main_program_state.original_video_mode);					
						textcolor(main_program_state.original_text_fg);
						bgcolor(main_program_state.original_text_bg);
						__asm__("clc"); 
						__asm__("jsr $ff47");  // enter_basic
					}
cancel_exit:										
					break;
			  }
				
			case 157:  // LEFT ARROW
			  {
					if (edit_index > 0)
					{
						// we're in edit mode, save the current value
						ch = hex2value(edit_str);
						hex_data_buffer[main_program_state.offset_min + main_program_state.curr_byte_idx] = ch;
						edit_index = 0;  
						edit_str[0] = '\0';
						edit_str[1] = '\0';
						// TBD: WRITE
					}
					
					main_program_state.curr_x -= main_program_state.byte_spacing;
					if (main_program_state.curr_x < FIRST_COLUMN)
					{
						main_program_state.curr_x = main_program_state.max_column;
						main_program_state.curr_byte_idx += (main_program_state.byte_per_row-1);
					}
					else
					{
						--main_program_state.curr_byte_idx;
					}					
			  }
			  break;
			case 29:   // RIGHT ARROW
			  {
					if (edit_index > 0)
					{
						// we're in edit mode, save the current value
						ch = hex2value(edit_str);						
						hex_data_buffer[main_program_state.offset_min + main_program_state.curr_byte_idx] = ch;
						edit_index = 0;  
						edit_str[0] = '\0';
						edit_str[1] = '\0';						
					}
					
					main_program_state.curr_x += main_program_state.byte_spacing;
					if (main_program_state.curr_x > main_program_state.max_column)
					{
						main_program_state.curr_x = FIRST_COLUMN;
						main_program_state.curr_byte_idx -= (main_program_state.byte_per_row-1);
					}
					else
					{
						++main_program_state.curr_byte_idx;
					}					
			  }
			  break;
			case 145:  // UP ARROW
			  {
					if (edit_index > 0)
					{
						// we're in edit mode, save the current value
						ch = hex2value(edit_str);
						hex_data_buffer[main_program_state.offset_min + main_program_state.curr_byte_idx] = ch;
						edit_index = 0;  
						edit_str[0] = '\0';
						edit_str[1] = '\0';
						// TBD: WRITE
					}
					
			    --main_program_state.curr_y;
					if (main_program_state.curr_y < main_program_state.row_top)
					{
						main_program_state.curr_y = main_program_state.row_top;
						goto LEFT_BRACKET;
					}
					else
					{
						main_program_state.curr_byte_idx -= main_program_state.byte_per_row;
					}
				}
			  break;
			case 17:   // DOWN ARROW
			  {
					if (edit_index > 0)
					{
						// we're in edit mode, save the current value
						ch = hex2value(edit_str);
						hex_data_buffer[main_program_state.offset_min + main_program_state.curr_byte_idx] = ch;
						edit_index = 0;  
						edit_str[0] = '\0';
						edit_str[1] = '\0';
						// TBD: WRITE
					}
					
			    ++main_program_state.curr_y;
					if (main_program_state.curr_y >= main_program_state.row_bottom)
					{
						main_program_state.curr_y = (main_program_state.row_bottom - 1);
						goto RIGHT_BRACKET;
					}
					else
					{
						main_program_state.curr_byte_idx += main_program_state.byte_per_row;
					}
				}
			  break;
				
      case 93:  // RIGHT BRACKET
			  {
RIGHT_BRACKET:					
					if (main_program_state.offset_max < (main_program_state.bytes_read+(2*main_program_state.byte_per_row)))
					{
					  main_program_state.offset_min += main_program_state.byte_per_row;
					  main_program_state.offset_max += main_program_state.byte_per_row;
					  force_full_refresh = TRUE;			
					}
			  }
        break;					
		  case 91: // LEFT BRACKET
			  {
LEFT_BRACKET:					
					if (main_program_state.offset_min >= main_program_state.byte_per_row)
					{
					  main_program_state.offset_min -= main_program_state.byte_per_row;
					  main_program_state.offset_max -= main_program_state.byte_per_row;
						force_full_refresh = TRUE;			
					}
					else
					{
						//reset_to_top();
						//force_full_refresh = TRUE;			
					}
			  }
				break;
				
			case 192:   // RIGHT BRACE
				break;
			case 186:   // LEFT BRACE
				break;
			case 130:  // PGUP
			  {
					if (edit_index > 0)
					{
						// do nothing
					}
					else 
					{
						input_ch = (main_program_state.curr_height/2) * main_program_state.byte_per_row;
						if (main_program_state.offset_min > input_ch)
						{
							main_program_state.offset_min -= input_ch;
							main_program_state.offset_max -= input_ch;
							force_full_refresh = TRUE;			
						}
						else
						{
							reset_to_top();
							force_full_refresh = TRUE;			
						}
					}
			  }
			  break;
			case 2:  // PGDN
			  {
					if (edit_index > 0)
					{
						// do nothing
					}
					else 
					{
						input_ch = (main_program_state.curr_height/2) * main_program_state.byte_per_row;
						if (main_program_state.offset_max < (main_program_state.bytes_read+input_ch))
						{
							main_program_state.offset_min += input_ch;
							main_program_state.offset_max += input_ch;
							force_full_refresh = TRUE;			
						}					
					}
			  }
			  break;
			case 19:  // HOME
			  {
					if (edit_index > 0)
					{
						// do nothing
					}
					else 
					{
            draw_main_screen_overlay(TRUE);
					}
			  }
			  break;
			case 4:  // END
				break;
				
		  case 148:  // insert
			  {
					if (edit_index > 0)
					{
						// do nothing
					}
					else 
					{
						if (
						  ((main_program_state.offset_min + main_program_state.curr_byte_idx) <= main_program_state.bytes_read)
						  && (main_program_state.bytes_read < MAX_BUFFER_SIZE)							
					  )
						{
							// copy everything from the current index to +1
							for (input_ch = main_program_state.bytes_read; input_ch > (main_program_state.offset_min + main_program_state.curr_byte_idx); --input_ch)
							{
								hex_data_buffer[input_ch+1] = hex_data_buffer[input_ch];
							}
							// input_ch is now == main_program_state.curr_byte_idx
							hex_data_buffer[input_ch+1] = hex_data_buffer[input_ch];						
							hex_data_buffer[input_ch] = 0x00;
							
							++main_program_state.bytes_read;
							
							update_bytes_read_display();
							main_program_state.buffer_modified = TRUE;
							force_full_refresh = TRUE;
						}					
					}
			  }
			  break;
				
			case 25:  // delete
			  {
					if (edit_index > 0)
					{
						// do nothing
					}
					else 
					{
						if (
						  ((main_program_state.offset_min + main_program_state.curr_byte_idx) <= main_program_state.bytes_read)
						  && (main_program_state.bytes_read > 0)  // some content is left to be deleted
					  )
						{
							// copy everything from the current index to +1
							for (input_ch = (main_program_state.offset_min + main_program_state.curr_byte_idx); input_ch < main_program_state.bytes_read-1; ++input_ch)
							{
								hex_data_buffer[input_ch] = hex_data_buffer[input_ch+1];
							}						
							
							--main_program_state.bytes_read;
							
							update_bytes_read_display();
							main_program_state.buffer_modified = TRUE;
							force_full_refresh = TRUE;
						}					
					}
			  }
			  break;
				
			default:
  			break;				
			}
			
update_hex_display:
			if (main_program_state.buffer_modified == TRUE)
			{
				//sprintf(str, "FILE: %s", main_program_state.input_filename);
				textcolor(COLOR_WHITE);
	      cputsxy(0, main_program_state.curr_height, "***");
			}
			else
			{
				cputsxy(0, main_program_state.curr_height, "   ");
			}			
		
      // Draw current byte index at bottom of screen
      print_xy_ulong_as_decimal(main_program_state.col_bytes_read, main_program_state.curr_height, main_program_state.offset_min + main_program_state.curr_byte_idx, CX16_BG_BLUE | CX16_FG_LT_RED, 8);

			// UPDATE HEX DISPLAY
	    idx_col = FIRST_COLUMN;
	    idx_row = 2;
			idx_bytes_this_row = 0;			
      
			for (idx = main_program_state.offset_min; idx < main_program_state.offset_max; ++idx)		  
			{				
				if (force_full_refresh == FALSE)
				{
					// The hex display was already mostly populated, we just need to
					// update the area around where the current cursor position is.
					if ( 
					  (idx_row < (main_program_state.curr_y-1))
						|| (idx_row > (main_program_state.curr_y+1))
				  )
					{
						goto skip_draw_update;
					}
				}
				
				if (idx > main_program_state.bytes_read)  // done with the contents of current file
				{
					ch = 0;
				}
				else
				{
					ch = hex_data_buffer[idx];
				}

				if (idx_col == FIRST_COLUMN)  // write the offset address only when we're on the first column of bytes
				{
					print_xy_ulong_as_hex(0, idx_row, idx, CX16_BG_BLACK | CX16_FG_YELLOW, 8);
				}
				
				if (
				  (idx_col == main_program_state.curr_x)
					&& (idx_row == main_program_state.curr_y)
			  )
				{
					if (edit_index > 0)
					{
						hex_color = CX16_BG_YELLOW | CX16_FG_RED;

            ch = hex2value(edit_str);
					}
					else
					{
					  hex_color = CX16_BG_YELLOW | CX16_FG_GREEN;
					}
				}
				else
				{
					if (idx > main_program_state.bytes_read)
					{
						hex_color = CX16_BG_BLACK | CX16_FG_DK_GRAY;
					}
					else
					{
				    if (idx % 2 == 0)
				    {
  				    hex_color = CX16_BG_BLACK | CX16_FG_CYAN;
				    }
				    else
				    {
    					hex_color = CX16_BG_BLACK | CX16_FG_WHITE;
				    }
					}
				}
        print_xy_ulong_as_hex(idx_col, idx_row, ch, hex_color, 2);				

        // Draw decode value
				textcolor(hex_color);
 			  if (
				  ((ch >= 32) && (ch <= 127))
					|| ((ch >= 160) && (ch < 255))
					|| (ch == 255)
				)
				{					
					cputcxy(main_program_state.decode_offset + idx_bytes_this_row, idx_row, ch);				  
				}
				else
				{
					cputcxy(main_program_state.decode_offset+idx_bytes_this_row, idx_row, '?');																				
				}
		
skip_draw_update:		
				++idx_bytes_this_row;
				if (idx_bytes_this_row == main_program_state.byte_per_row)
				{
					// GO TO THE NEXT COLUMN
					idx_col = FIRST_COLUMN;
					++idx_row;
          idx_bytes_this_row = 0;					
				}
				else
				{
					idx_col += main_program_state.byte_spacing;
				}
				
				//if (idx_row > main_program_state.row_bottomP1)  // if out of space to draw on...
				//{
				//	break;
				//}
				
			}
  		force_full_refresh = FALSE;			
		}
			
		ch = 0;
	}  // end while (TRUE) (main loop)

}
