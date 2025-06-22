#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <conio.h>
#include <windows.h>
#include <ctype.h>

#define ALPHABET_SIZE 256
#define MAX_SUGGESTIONS 15
#define MAX_CODE_SIZE 16384
#define MAX_LINE_SIZE 512
#define MAX_LINES 100

typedef struct TrieNode {
    struct TrieNode* children[ALPHABET_SIZE];
    int is_end_of_word;
} TrieNode;

typedef struct {
    char text[MAX_LINE_SIZE];
    int length;
} EditorLine;

EditorLine lines[MAX_LINES];
int current_line = 0;
int cursor_pos = 0;
int total_lines = 1;
TrieNode* knowledge_base;
int show_suggestions = 0;
char suggestions[MAX_SUGGESTIONS][MAX_LINE_SIZE];
int suggestion_count = 0;
int selected_suggestion = -1;
int arrow_key_mode = 0;

HANDLE hConsole = INVALID_HANDLE_VALUE;
COORD bufferSize = {120, 30};
CHAR_INFO* buffer = NULL;
HANDLE hBuffer = INVALID_HANDLE_VALUE;

void init_console() {
    hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTitle("MintMind C Editor");
    
    hBuffer = CreateConsoleScreenBuffer(
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        CONSOLE_TEXTMODE_BUFFER,
        NULL);
    
    if (hBuffer != INVALID_HANDLE_VALUE) {
        SetConsoleActiveScreenBuffer(hBuffer);
        SetConsoleScreenBufferSize(hBuffer, bufferSize);
    } else {
        hBuffer = hConsole;
    }
    
    CONSOLE_CURSOR_INFO cursorInfo;
    GetConsoleCursorInfo(hBuffer, &cursorInfo);
    cursorInfo.bVisible = TRUE;
    cursorInfo.dwSize = 100;
    SetConsoleCursorInfo(hBuffer, &cursorInfo);
    
    buffer = (CHAR_INFO*)malloc(bufferSize.X * bufferSize.Y * sizeof(CHAR_INFO));
    if (!buffer) {
        MessageBox(NULL, "Memory allocation failed", "Error", MB_OK);
        exit(1);
    }
}

void cleanup_console() {
    if (buffer) free(buffer);
    if (hBuffer != INVALID_HANDLE_VALUE && hBuffer != hConsole) {
        SetConsoleActiveScreenBuffer(hConsole);
        CloseHandle(hBuffer);
    }
}

void clear_buffer() {
    if (!buffer) return;
    
    for (int y = 0; y < bufferSize.Y; y++) {
        for (int x = 0; x < bufferSize.X; x++) {
            int pos = y * bufferSize.X + x;
            buffer[pos].Char.UnicodeChar = ' ';
            buffer[pos].Attributes = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
        }
    }
}

void write_buffer() {
    if (!buffer || hBuffer == INVALID_HANDLE_VALUE) return;
    
    COORD bufferCoord = {0, 0};
    SMALL_RECT writeRegion = {0, 0, bufferSize.X-1, bufferSize.Y-1};
    WriteConsoleOutput(hBuffer, buffer, bufferSize, bufferCoord, &writeRegion);
}

void set_buffer_char(int x, int y, char c, WORD attr) {
    if (x >= 0 && x < bufferSize.X && y >= 0 && y < bufferSize.Y) {
        int pos = y * bufferSize.X + x;
        buffer[pos].Char.AsciiChar = c;
        buffer[pos].Attributes = attr;
    }
}

void set_buffer_text(int x, int y, const char* text, WORD attr) {
    for (int i = 0; text[i] && x + i < bufferSize.X; i++) {
        set_buffer_char(x + i, y, text[i], attr);
    }
}

TrieNode* create_node() {
    TrieNode* node = (TrieNode*)malloc(sizeof(TrieNode));
    if (node) {
        node->is_end_of_word = 0;
        memset(node->children, 0, sizeof(node->children));
    }
    return node;
}

void trie_insert(TrieNode* root, const char* key) {
    if (!root || !key) return;
    
    TrieNode* current = root;
    for (int i = 0; key[i] != '\0'; i++) {
        int index = (unsigned char)key[i];
        if (!current->children[index]) {
            current->children[index] = create_node();
            if (!current->children[index]) return;
        }
        current = current->children[index];
    }
    current->is_end_of_word = 1;
}

void free_trie(TrieNode* root) {
    if (!root) return;
    
    for (int i = 0; i < ALPHABET_SIZE; i++) {
        if (root->children[i]) {
            free_trie(root->children[i]);
        }
    }
    free(root);
}

void get_suggestions(TrieNode* node, char* buffer, int level) {
    if (node->is_end_of_word && suggestion_count < MAX_SUGGESTIONS) {
        buffer[level] = '\0';
        strcpy(suggestions[suggestion_count++], buffer);
    }

    for (int i = 0; i < ALPHABET_SIZE && suggestion_count < MAX_SUGGESTIONS; i++) {
        if (node->children[i]) {
            buffer[level] = (char)i;
            get_suggestions(node->children[i], buffer, level + 1);
        }
    }
}

void get_suggestions_at_pos(TrieNode* root, const char* prefix) {
    suggestion_count = 0;
    selected_suggestion = -1;
    memset(suggestions, 0, sizeof(suggestions));

    TrieNode* node = root;
    for (int i = 0; prefix[i]; i++) {
        int index = (unsigned char)prefix[i];
        if (!node->children[index]) return;
        node = node->children[index];
    }

    char buffer[MAX_LINE_SIZE] = {0};
    strcpy(buffer, prefix);
    int len = strlen(prefix);

    for (int i = 0; i < ALPHABET_SIZE && suggestion_count < MAX_SUGGESTIONS; i++) {
        if (node->children[i]) {
            buffer[len] = (char)i;
            get_suggestions(node->children[i], buffer, len + 1);
        }
    }
}

void init_c_knowledge() {
    knowledge_base = create_node();
    if (!knowledge_base) return;

    const char* knowledge[] = {
        "stdio.h", "stdlib.h", "string.h", "math.h", "time.h", 
        "ctype.h", "stdbool.h", "limits.h", "float.h",
        "#include", "#define", "#ifdef", "#ifndef", "#endif",
        "#pragma", "#if", "#else", "#elif",
        "printf", "scanf", "fopen", "fclose", "malloc", "free",
        "calloc", "realloc", "exit", "atoi", "atof", "rand", "srand",
        "system", "abs", "strcpy", "strcat", "strcmp", "strlen",
        "memcpy", "memset", "sin", "cos", "tan", "sqrt", "pow", "log",
        "time", "clock", "sizeof", "main",
        "auto", "break", "case", "char", "const", "continue", "default",
        "do", "double", "else", "enum", "extern", "float", "for", "goto",
        "if", "int", "long", "register", "return", "short", "signed",
        "sizeof", "static", "struct", "switch", "typedef", "union",
        "unsigned", "void", "volatile", "while",
        "for(int i=0; i<n; i++)", "while(1)", "if()", "else if()", 
        "switch()", "case", "break;", "continue;", "return 0;", 
        "NULL", "FILE*", "size_t", "typedef struct", "void*", "int main()",
        NULL
    };

    for (int i = 0; knowledge[i] != NULL; i++) {
        trie_insert(knowledge_base, knowledge[i]);
    }
}

void display_editor() {
    clear_buffer();
    
    set_buffer_text(0, 0, " MintMind C Editor ", 
                   FOREGROUND_GREEN | FOREGROUND_INTENSITY);
    
    char lineInfo[30];
    sprintf(lineInfo, "(Line %d/%d)", current_line + 1, total_lines);
    set_buffer_text(20, 0, lineInfo, 
                   FOREGROUND_GREEN | FOREGROUND_INTENSITY);
    
    set_buffer_text(0, 1, "================================",
                   FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);

    int start = (current_line > 5) ? current_line - 5 : 0;
    int end = (current_line + 6 < total_lines) ? current_line + 6 : total_lines;
    int display_line = 2;

    for (int i = start; i < end; i++, display_line++) {
        if (i == current_line) {
            set_buffer_text(0, display_line, ">", 
                          BACKGROUND_BLUE | BACKGROUND_INTENSITY);
        } else {
            set_buffer_text(0, display_line, " ", 
                          FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
        }

        const char* text = lines[i].text;
        int x = 2;
        
        if (strstr(text, "#include") == text) {
            set_buffer_text(x, display_line, "#include", 
                          FOREGROUND_GREEN | FOREGROUND_INTENSITY);
            x += 8;
            set_buffer_text(x, display_line, text + 8, 
                          FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
        } 
        else if (strstr(text, "printf") == text) {
            set_buffer_text(x, display_line, "printf", 
                          FOREGROUND_BLUE | FOREGROUND_INTENSITY);
            x += 6;
            set_buffer_text(x, display_line, text + 6, 
                          FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
        }
        else if (strstr(text, "//") == text) {
            set_buffer_text(x, display_line, text, FOREGROUND_GREEN);
        }
        else {
            set_buffer_text(x, display_line, text, 
                          FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
        }
    }

    if (show_suggestions && suggestion_count > 0) {
        int suggestion_y = display_line + 1;
        set_buffer_text(0, suggestion_y, "Suggestions:", 
                       FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
        suggestion_y++;
        
        for (int i = 0; i < suggestion_count && suggestion_y < bufferSize.Y - 2; i++, suggestion_y++) {
            WORD attr = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
            if (i == selected_suggestion) {
                attr = BACKGROUND_GREEN | BACKGROUND_INTENSITY;
            }
            set_buffer_text(2, suggestion_y, suggestions[i], attr);
        }
    }

    int status_y = bufferSize.Y - 2;
    set_buffer_text(0, status_y, 
                   "F1:Save/Run  F2:NewLine  F3:Help  TAB:Suggestions  ESC:Exit",
                   FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
    
    char posInfo[20];
    sprintf(posInfo, "Line %d, Col %d", current_line + 1, cursor_pos + 1);
    set_buffer_text(0, bufferSize.Y - 1, posInfo,
                   FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);

    write_buffer();
    
    COORD cursorPos = {cursor_pos + 2, current_line - start + 2};
    SetConsoleCursorPosition(hBuffer, cursorPos);
}

void insert_char(char ch) {
    if (cursor_pos < MAX_LINE_SIZE - 1) {
        memmove(&lines[current_line].text[cursor_pos + 1], 
                &lines[current_line].text[cursor_pos], 
                lines[current_line].length - cursor_pos + 1);
        lines[current_line].text[cursor_pos] = ch;
        lines[current_line].length++;
        cursor_pos++;
    }
}

void delete_char() {
    if (cursor_pos > 0) {
        memmove(&lines[current_line].text[cursor_pos - 1], 
                &lines[current_line].text[cursor_pos], 
                lines[current_line].length - cursor_pos + 1);
        lines[current_line].length--;
        cursor_pos--;
    }
}

void new_line() {
    if (total_lines < MAX_LINES - 1) {
        for (int i = total_lines; i > current_line + 1; i--) {
            strcpy(lines[i].text, lines[i-1].text);
            lines[i].length = lines[i-1].length;
        }
        
        strcpy(lines[current_line + 1].text, &lines[current_line].text[cursor_pos]);
        lines[current_line + 1].length = strlen(lines[current_line + 1].text);
        lines[current_line].text[cursor_pos] = '\0';
        lines[current_line].length = cursor_pos;
        
        total_lines++;
        current_line++;
        cursor_pos = 0;
    }
}

void apply_suggestion() {
    if (selected_suggestion >= 0 && selected_suggestion < suggestion_count) {
        int word_start = cursor_pos;
        while (word_start > 0 && lines[current_line].text[word_start - 1] != ' ' && 
               lines[current_line].text[word_start - 1] != '\t' && 
               lines[current_line].text[word_start - 1] != '\n') {
            word_start--;
        }
        
        int partial_len = cursor_pos - word_start;
        int suggestion_len = strlen(suggestions[selected_suggestion]);
        
        if (lines[current_line].length + suggestion_len - partial_len < MAX_LINE_SIZE) {
            memmove(&lines[current_line].text[word_start + suggestion_len], 
                    &lines[current_line].text[cursor_pos], 
                    lines[current_line].length - cursor_pos + 1);
            memcpy(&lines[current_line].text[word_start], 
                   suggestions[selected_suggestion], 
                   suggestion_len);
            lines[current_line].length += suggestion_len - partial_len;
            cursor_pos = word_start + suggestion_len;
        }
    }
    show_suggestions = 0;
}

void execute_program() {
    FILE* f = fopen("program.c", "w");
    if (!f) return;
    
    for (int i = 0; i < total_lines; i++) {
        fputs(lines[i].text, f);
        fputc('\n', f);
    }
    fclose(f);
    
    clear_buffer();
    set_buffer_text(0, 0, "Compiling and running program...", 
                   FOREGROUND_GREEN | FOREGROUND_INTENSITY);
    set_buffer_text(0, 1, "Please check the new console window for output", 
                   FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
    write_buffer();
    
    // Create a proper process with visible console
    STARTUPINFO si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    char cmd[] = "cmd.exe /c gcc program.c -o program.exe && program.exe && pause";
    
    if (CreateProcess(NULL, cmd, NULL, NULL, FALSE, 
                     CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, INFINITE);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    } else {
        set_buffer_text(0, 3, "Failed to create process!", 
                       FOREGROUND_RED | FOREGROUND_INTENSITY);
        write_buffer();
    }
    
    set_buffer_text(0, 5, "Press any key to continue editing...", 
                   FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
    write_buffer();
    _getch();
}

int main() {
    memset(lines, 0, sizeof(lines));
    init_c_knowledge();
    lines[0].length = 0;
    
    init_console();

    while (1) {
        display_editor();
        
        if (_kbhit()) {
            int ch = _getch();
            
            if (ch == 0 || ch == 224) {
                arrow_key_mode = 1;
                ch = _getch();
                switch (ch) {
                    case 59:  // F1
                        execute_program();
                        continue;
                        
                    case 60:  // F2
                        new_line();
                        continue;
                        
                    case 61:  // F3
                        clear_buffer();
                        set_buffer_text(0, 0, "MintMind C Editor Help", 
                                       FOREGROUND_GREEN | FOREGROUND_INTENSITY);
                        set_buffer_text(0, 1, "=====================", 
                                       FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
                        set_buffer_text(0, 3, "Arrow Keys: Navigate", 
                                       FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
                        set_buffer_text(0, 4, "Enter:     New Line", 
                                       FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
                        set_buffer_text(0, 5, "F1:        Save & Run", 
                                       FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
                        set_buffer_text(0, 6, "F2:        New Line", 
                                       FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
                        set_buffer_text(0, 7, "TAB:       Suggestions", 
                                       FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
                        set_buffer_text(0, 8, "ESC:       Exit", 
                                       FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
                        set_buffer_text(0, 10, "Press any key to continue...", 
                                       FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
                        write_buffer();
                        _getch();
                        continue;
                        
                    case 72:  // Up
                        if (show_suggestions && suggestion_count > 0) {
                            selected_suggestion = (selected_suggestion > 0) ? 
                                selected_suggestion - 1 : suggestion_count - 1;
                        } else if (current_line > 0) {
                            current_line--;
                            if (cursor_pos > lines[current_line].length) {
                                cursor_pos = lines[current_line].length;
                            }
                        }
                        continue;
                        
                    case 80:  // Down
                        if (show_suggestions && suggestion_count > 0) {
                            selected_suggestion = (selected_suggestion < suggestion_count - 1) ? 
                                selected_suggestion + 1 : 0;
                        } else if (current_line < total_lines - 1) {
                            current_line++;
                            if (cursor_pos > lines[current_line].length) {
                                cursor_pos = lines[current_line].length;
                            }
                        }
                        continue;
                        
                    case 75:  // Left
                        if (cursor_pos > 0) cursor_pos--;
                        continue;
                        
                    case 77:  // Right
                        if (cursor_pos < lines[current_line].length) cursor_pos++;
                        continue;
                }
            }
            
            switch (ch) {
                case 27:  // ESC
                    cleanup_console();
                    free_trie(knowledge_base);
                    return 0;
                    
                case 13:  // Enter
                    if (show_suggestions && selected_suggestion >= 0) {
                        apply_suggestion();
                    } else {
                        new_line();
                    }
                    continue;
                    
                case '\t':  // Tab
                    if (!show_suggestions) {
                        int word_start = cursor_pos;
                        while (word_start > 0 && lines[current_line].text[word_start - 1] != ' ' && 
                               lines[current_line].text[word_start - 1] != '\t' && 
                               lines[current_line].text[word_start - 1] != '\n') {
                            word_start--;
                        }
                        
                        char current_word[MAX_LINE_SIZE] = {0};
                        strncpy(current_word, &lines[current_line].text[word_start], 
                                cursor_pos - word_start);
                        
                        get_suggestions_at_pos(knowledge_base, current_word);
                        show_suggestions = 1;
                        selected_suggestion = (suggestion_count > 0) ? 0 : -1;
                    } else if (suggestion_count > 0) {
                        apply_suggestion();
                    }
                    continue;
                    
                case '\b':  // Backspace
                    delete_char();
                    show_suggestions = 0;
                    continue;
                    
                default:
                    if (arrow_key_mode) {
                        arrow_key_mode = 0;
                        continue;
                    }
                    
                    if (ch >= 32 && ch <= 126) {
                        insert_char(ch);
                        
                        if (isalpha(ch) || ch == '#' || ch == '_') {
                            int word_start = cursor_pos - 1;
                            while (word_start > 0 && lines[current_line].text[word_start - 1] != ' ' && 
                                   lines[current_line].text[word_start - 1] != '\t' && 
                                   lines[current_line].text[word_start - 1] != '\n') {
                                word_start--;
                            }
                            
                            char current_word[MAX_LINE_SIZE] = {0};
                            strncpy(current_word, &lines[current_line].text[word_start], 
                                    cursor_pos - word_start);
                            
                            get_suggestions_at_pos(knowledge_base, current_word);
                            show_suggestions = 1;
                            selected_suggestion = (suggestion_count > 0) ? 0 : -1;
                        } else {
                            show_suggestions = 0;
                        }
                    }
            }
        }
    }
}
