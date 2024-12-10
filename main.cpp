#include <windows.h>
#include <commdlg.h>
#include <commctrl.h>
#include <cstdio>
#include <iostream> // Incluído para depuração
#include "resource.h"

#define ID_FILE_OPEN  1
#define ID_FILE_SAVE  2
#define ID_FILE_NEW   3
#define ID_FILE_EXIT  4
#define ID_EDIT_TEXT  101
#define IDI_APP_ICON  101

static HWND hStatus; // Barra de status
static HWND hEdit;   // Área de edição
static char currentFilePath[MAX_PATH] = ""; // Caminho do arquivo atual

/////////////////////////////////////////////////////////////////////////////////
// Função para atualizar a barra de status com linha e coluna                  //
/////////////////////////////////////////////////////////////////////////////////
void UpdateStatusBar(const char* fileStatus, int line = -1, int col = -1) {
    char statusText[128];

    // Exibe linha e coluna se disponíveis
    if (line >= 0 && col >= 0) {
        sprintf(statusText, "%s | Linha: %d, Coluna: %d", fileStatus, line + 1, col + 1);
    } else {
        strcpy(statusText, fileStatus); // Exibe apenas o texto do arquivo
    }

    //std::cout << "Atualizando barra de status: " << statusText << std::endl; // Depuração
    SendMessage(hStatus, SB_SETTEXT, 0, (LPARAM)statusText);
}

/////////////////////////////////////////////////////////////////////////////////
// Função para calcular linha e coluna com base na posição do cursor           //
/////////////////////////////////////////////////////////////////////////////////
void UpdateCursorPos(HWND hwndEdit) {
    DWORD selStart, selEnd;

    // Obtém a posição do cursor (posição inicial e final da seleção)
    SendMessage(hwndEdit, EM_GETSEL, (WPARAM)&selStart, (LPARAM)&selEnd);

    // Calcula a linha correspondente à posição do cursor
    int line = SendMessage(hwndEdit, EM_LINEFROMCHAR, selStart, 0);

    // Calcula a posição inicial da linha
    int lineStart = SendMessage(hwndEdit, EM_LINEINDEX, line, 0);

    // Calcula a coluna
    int col = selStart - lineStart;

    // Depuração: Mostra os valores calculados
    //std::cout << "UpdateCursorPos chamado: Linha = " << line + 1 << ", Coluna = " << col + 1 << std::endl;

    // Atualiza a barra de status com as informações de linha e coluna
    UpdateStatusBar(currentFilePath[0] ? currentFilePath : "Sem arquivo", line, col);
}

/////////////////////////////////////////////////////////////////////////////////
// Função para salvar o texto no arquivo atual ou em um novo arquivo           //
/////////////////////////////////////////////////////////////////////////////////
void SaveFile(HWND hwnd) {
    if (strlen(currentFilePath) == 0) { // Se não houver arquivo atual, abrir "Salvar Como"
        OPENFILENAME ofn;
        char fileName[MAX_PATH] = "";

        ZeroMemory(&ofn, sizeof(ofn));
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = hwnd;
        ofn.lpstrFilter = "Text Files\0*.txt\0All Files\0*.*\0";
        ofn.lpstrFile = fileName;
        ofn.nMaxFile = MAX_PATH;
        ofn.Flags = OFN_OVERWRITEPROMPT;

        if (GetSaveFileName(&ofn)) {
            strcpy(currentFilePath, fileName); // Atualiza o caminho do arquivo
        } else {
            return; // O usuário cancelou a operação
        }
    }

    // Salva o texto no arquivo atual
    int length = GetWindowTextLength(hEdit); // Obtém o tamanho do texto
    char* buffer = (char*)malloc(length + 1);
    GetWindowText(hEdit, buffer, length + 1); // Obtém o texto do editor

    FILE* file = fopen(currentFilePath, "wb");
    if (file) {
        fwrite(buffer, 1, strlen(buffer), file); // Escreve o texto no arquivo
        fclose(file);
        //std::cout << "Arquivo salvo: " << currentFilePath << std::endl; // Depuração
    }

    free(buffer);

    UpdateStatusBar(currentFilePath); // Atualiza a barra de status
}

/////////////////////////////////////////////////////////////////////////////////
// Callback para o controle de edição (Captura teclas direcionais)             //
/////////////////////////////////////////////////////////////////////////////////
LRESULT CALLBACK EditProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    static WNDPROC oldProc = (WNDPROC)GetWindowLongPtr(hwnd, GWLP_USERDATA);

    switch (uMsg) {
        case WM_KEYUP: // Captura teclas como Up, Down, Left, Right
            //std::cout << "WM_KEYUP no controle de edição: Tecla pressionada." << std::endl; // Depuração
            UpdateCursorPos(hwnd);
            break;
    }

    return CallWindowProc(oldProc, hwnd, uMsg, wParam, lParam);
}

/////////////////////////////////////////////////////////////////////////////////
// Callback para processar eventos da janela principal                         //
/////////////////////////////////////////////////////////////////////////////////
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE: {
            hEdit = CreateWindow(
                "EDIT", "",
                WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL,
                0, 0, 0, 0,
                hwnd, (HMENU) ID_EDIT_TEXT, NULL, NULL
            );

            // Substitui o procedimento padrão do controle de edição
            SetWindowLongPtr(hEdit, GWLP_USERDATA, (LONG_PTR)SetWindowLongPtr(hEdit, GWLP_WNDPROC, (LONG_PTR)EditProc));

            hStatus = CreateWindowEx(
                0, STATUSCLASSNAME, NULL,
                WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
                0, 0, 0, 0,
                hwnd, NULL, GetModuleHandle(NULL), NULL
            );

            UpdateStatusBar("Pronto");
        }
        break;

        case WM_SIZE: {
            RECT rect;
            GetClientRect(hwnd, &rect);

            int statusHeight = 20; // Altura da barra de status
            int editHeight = rect.bottom - statusHeight;

            MoveWindow(hEdit, 0, 0, rect.right, editHeight, TRUE);
            MoveWindow(hStatus, 0, editHeight, rect.right, statusHeight, TRUE);
        }
        break;

        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case ID_FILE_NEW:
                    SetWindowText(hEdit, "");
                    strcpy(currentFilePath, "");
                    UpdateStatusBar("Novo arquivo");
                    break;

                case ID_FILE_OPEN: {
                    char fileName[MAX_PATH] = "";
                    OPENFILENAME ofn = { sizeof(ofn) };
                    ofn.hwndOwner = hwnd;
                    ofn.lpstrFilter = "Text Files\0*.txt\0All Files\0*.*\0";
                    ofn.lpstrFile = fileName;
                    ofn.nMaxFile = MAX_PATH;

                    if (GetOpenFileName(&ofn)) {
                        FILE* file = fopen(fileName, "rb");
                        if (file) {
                            fseek(file, 0, SEEK_END);
                            long fileSize = ftell(file);
                            rewind(file);

                            char* buffer = (char*)malloc(fileSize + 1);
                            fread(buffer, 1, fileSize, file);
                            buffer[fileSize] = '\0';
                            fclose(file);

                            SetWindowText(hEdit, buffer);
                            free(buffer);

                            strcpy(currentFilePath, fileName);
                        }
                    }
                }
                break;

                case ID_FILE_SAVE:
                    SaveFile(hwnd);
                    break;

                case ID_FILE_EXIT:
                    PostQuitMessage(0);
                    break;
            }
            break;

        case WM_DESTROY:
            PostQuitMessage(0);
            break;

        default:
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

/////////////////////////////////////////////////////////////////////////////////
// Função principal                                                            //
/////////////////////////////////////////////////////////////////////////////////
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    const char CLASS_NAME[] = "EditorSimples";

    WNDCLASSEX wc = {0};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;

    wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_APP_ICON));
    wc.hIconSm = (HICON)LoadImage(hInstance, MAKEINTRESOURCE(IDI_APP_ICON), IMAGE_ICON, 16, 16, 0);

    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

    RegisterClassEx(&wc);

    HMENU hMenu = CreateMenu();
    HMENU hFileMenu = CreateMenu();
    AppendMenu(hFileMenu, MF_STRING, ID_FILE_OPEN, "Abrir\tCtrl-O");
    AppendMenu(hFileMenu, MF_STRING, ID_FILE_SAVE, "Salvar\tCtrl-S");
    AppendMenu(hFileMenu, MF_STRING, ID_FILE_NEW, "Novo\tCtrl-N");
    AppendMenu(hFileMenu, MF_STRING, ID_FILE_EXIT, "Sair\tCtrl-Q");
    AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hFileMenu, "Arquivo");

    ACCEL accTable[] = {
        { FCONTROL | FVIRTKEY, 'O', ID_FILE_OPEN },
        { FCONTROL | FVIRTKEY, 'S', ID_FILE_SAVE },
        { FCONTROL | FVIRTKEY, 'N', ID_FILE_NEW },
        { FCONTROL | FVIRTKEY, 'Q', ID_FILE_EXIT }
    };
    HACCEL hAccel = CreateAcceleratorTable(accTable, 4);

    HWND hwnd = CreateWindowEx(
        0, CLASS_NAME, "Editor de Texto Simples",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 600,
        NULL, hMenu, hInstance, NULL
    );

    ShowWindow(hwnd, nCmdShow);

    MSG msg = {0};
    while (GetMessage(&msg, NULL, 0, 0)) {
        if (!TranslateAccelerator(hwnd, hAccel, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return 0;
}
