#include <windows.h>  // Necessário para a API do Windows.
#include <commdlg.h>  // Necessário para diálogos comuns como Abrir e Salvar.
#include <commctrl.h> // Necessário para controles como a barra de status.
#include <cstdio>     // Necessário para funções C de manipulação de arquivos (fopen, fread, etc.).
#include <iostream>   // Necessário para depuração com std::cout.
#include <string>     // Necessário para manipulação de strings modernas em C++.
#include <algorithm>  // Para std::min
#include "resource.h"

// Identificadores de Menus
#define ID_FILE_OPEN  1
#define ID_FILE_SAVE  2
#define ID_FILE_NEW   3
#define ID_FILE_EXIT  4

// Identificadores de Controles
#define ID_EDIT_TEXT  101

// Identificadores de Recursos
#define IDI_APP_ICON  101

// Identificadores de Pesquisa e Substituição
#define ID_FIND       201
#define ID_REPLACE    202


static HWND hStatus; // Barra de status
static HWND hEdit;   // Área de edição
static char currentFilePath[MAX_PATH] = ""; // Caminho do arquivo atual


/////////////////////////////////////////////////////////////////////////////////
// Função para abrir o diálogo de pesquisa                                     //
/////////////////////////////////////////////////////////////////////////////////
void FindText( HWND hwnd ) {
    char findText[128] = "";

    // Cria uma caixa de diálogo para entrada do texto a ser pesquisado
    if ( DialogBoxParam( GetModuleHandle( NULL ), MAKEINTRESOURCE( ID_FIND ), hwnd, []( HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam ) -> INT_PTR {
    static char* pFindText = nullptr;
    switch ( uMsg ) {
        case WM_INITDIALOG:
            pFindText = reinterpret_cast<char*>( lParam );
            return TRUE;

        case WM_COMMAND:
            if ( LOWORD( wParam ) == IDOK ) {
                GetDlgItemText( hDlg, IDC_EDIT1, pFindText, 128 );

                // Depuração: Mostra o texto capturado do diálogo
                //std::cout << "Texto capturado do diálogo: " << pFindText << std::endl;

                EndDialog( hDlg, IDOK );
            } else if ( LOWORD( wParam ) == IDCANCEL ) {
                EndDialog( hDlg, IDCANCEL );
            }
            break;
        }
        return FALSE;
    }, ( LPARAM )findText ) == IDOK ) {
        // Localiza o texto no controle de edição
        int length = GetWindowTextLength( hEdit );
        char* buffer = ( char* )malloc( length + 1 );
        GetWindowText( hEdit, buffer, length + 1 );

        // Localiza o texto no editor
        char* pos = strstr( buffer, findText );
        if ( pos ) {
            int start = pos - buffer;
            int end = start + strlen( findText );

            // Validar os índices
            if ( start >= 0 && end <= length ) {
                // Seleciona o texto encontrado no controle de edição
                SendMessage( hEdit, EM_SETSEL, start, end );

                // Move o cursor para a posição do texto encontrado
                SendMessage( hEdit, EM_SCROLLCARET, 0, 0 );

                // Retorna o foco para o controle de edição
                SetFocus( hEdit );

                //std::cout << "Texto destacado: Início = " << start
                //          << ", Fim = " << end << std::endl;
            } else {
                //std::cerr << "Erro: Índices de seleção inválidos ("
                //          << start << ", " << end << ")." << std::endl;
            }
        } else {
            MessageBox( hwnd, "Texto não encontrado.", "Pesquisa", MB_OK | MB_ICONINFORMATION );
            //std::cout << "Texto não encontrado: " << findText << std::endl;
        }

        free( buffer );
    }
}


/////////////////////////////////////////////////////////////////////////////////
// Função para abrir o diálogo de substituição                                 //
/////////////////////////////////////////////////////////////////////////////////
void ReplaceText( HWND hwnd ) {
    struct ReplaceData {
        char findText[128];
        char replaceText[128];
    } replaceData = { "", "" };

    if ( DialogBoxParam( GetModuleHandle( NULL ), MAKEINTRESOURCE( ID_REPLACE ), hwnd, []( HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam ) -> INT_PTR {
    static ReplaceData* pReplaceData = nullptr;
    switch ( uMsg ) {
        case WM_INITDIALOG:
            pReplaceData = reinterpret_cast<ReplaceData*>( lParam );
            return TRUE;

        case WM_COMMAND:
            if ( LOWORD( wParam ) == IDOK ) {
                GetDlgItemText( hDlg, IDC_EDIT1, pReplaceData->findText, 128 );
                GetDlgItemText( hDlg, IDC_EDIT2, pReplaceData->replaceText, 128 );
                EndDialog( hDlg, IDOK );
            } else if ( LOWORD( wParam ) == IDCANCEL ) {
                EndDialog( hDlg, IDCANCEL );
            }
            break;
        }
        return FALSE;
    }, ( LPARAM )&replaceData ) == IDOK ) {
        // Substitui o texto no controle de edição
        int length = GetWindowTextLength( hEdit );
        char* buffer = ( char* )malloc( length + 1 );
        GetWindowText( hEdit, buffer, length + 1 );

        char* pos = strstr( buffer, replaceData.findText );
        if ( pos ) {
            int start = pos - buffer;
            int end = start + strlen( replaceData.findText );
            SendMessage( hEdit, EM_SETSEL, start, end );
            SendMessage( hEdit, EM_REPLACESEL, TRUE, ( LPARAM )replaceData.replaceText );
            MessageBox( hwnd, "Texto substituído com sucesso.", "Substituição", MB_OK | MB_ICONINFORMATION );
        } else {
            MessageBox( hwnd, "Texto não encontrado.", "Substituição", MB_OK | MB_ICONINFORMATION );
        }

        free( buffer );
    }
}

/////////////////////////////////////////////////////////////////////////////////
// Função para atualizar a barra de status com linha e coluna                  //
/////////////////////////////////////////////////////////////////////////////////
void UpdateStatusBar( const char* fileStatus, int line = -1, int col = -1 ) {
    char statusText[128];

    // Exibe linha e coluna se disponíveis
    if ( line >= 0 && col >= 0 ) {
        sprintf( statusText, "%s | Linha: %d, Coluna: %d", fileStatus, line + 1, col + 1 );
    } else {
        strcpy( statusText, fileStatus ); // Exibe apenas o texto do arquivo
    }

    //std::cout << "Atualizando barra de status: " << statusText << std::endl; // Depuração
    SendMessage( hStatus, SB_SETTEXT, 0, ( LPARAM )statusText );
}

/////////////////////////////////////////////////////////////////////////////////
// Função para calcular linha e coluna com base na posição do cursor           //
/////////////////////////////////////////////////////////////////////////////////
void UpdateCursorPos( HWND hwndEdit ) {
    DWORD selStart, selEnd;

    // Obtém a posição do cursor (posição inicial e final da seleção)
    SendMessage( hwndEdit, EM_GETSEL, ( WPARAM )&selStart, ( LPARAM )&selEnd );

    // Calcula a linha correspondente à posição do cursor
    int line = SendMessage( hwndEdit, EM_LINEFROMCHAR, selStart, 0 );

    // Calcula a posição inicial da linha
    int lineStart = SendMessage( hwndEdit, EM_LINEINDEX, line, 0 );

    // Calcula a coluna
    int col = selStart - lineStart;

    // Atualiza a barra de status com as informações de linha e coluna
    UpdateStatusBar( currentFilePath[0] ? currentFilePath : "Sem arquivo", line, col );
}

/////////////////////////////////////////////////////////////////////////////////
// Função para salvar o texto no arquivo atual ou em um novo arquivo           //
/////////////////////////////////////////////////////////////////////////////////
void SaveFile( HWND hwnd ) {
    if ( strlen( currentFilePath ) == 0 ) { // Se não houver arquivo atual, abrir "Salvar Como"
        OPENFILENAME ofn;
        char fileName[MAX_PATH] = "";

        ZeroMemory( &ofn, sizeof( ofn ) );
        ofn.lStructSize = sizeof( ofn );
        ofn.hwndOwner = hwnd;
        ofn.lpstrFilter = "Text Files\0*.txt\0All Files\0*.*\0";
        ofn.lpstrFile = fileName;
        ofn.nMaxFile = MAX_PATH;
        ofn.Flags = OFN_OVERWRITEPROMPT;

        if ( GetSaveFileName( &ofn ) ) {
            strcpy( currentFilePath, fileName ); // Atualiza o caminho do arquivo
        } else {
            return; // O usuário cancelou a operação
        }
    }

    // Salva o texto no arquivo atual
    int length = GetWindowTextLength( hEdit ); // Obtém o tamanho do texto
    char* buffer = ( char* )malloc( length + 1 );
    GetWindowText( hEdit, buffer, length + 1 ); // Obtém o texto do editor

    FILE* file = fopen( currentFilePath, "wb" );
    if ( file ) {
        fwrite( buffer, 1, strlen( buffer ), file ); // Escreve o texto no arquivo
        fclose( file );
        //std::cout << "Arquivo salvo: " << currentFilePath << std::endl; // Depuração
    }

    free( buffer );

    UpdateStatusBar( currentFilePath ); // Atualiza a barra de status
}

/////////////////////////////////////////////////////////////////////////////////
// Função para Abrir e Ler Arquivos em Blocos                                  //
/////////////////////////////////////////////////////////////////////////////////
void OpenFileLarge( HWND hwnd ) {
    OPENFILENAME ofn;
    char fileName[MAX_PATH] = "";

    ZeroMemory( &ofn, sizeof( ofn ) );
    ofn.lStructSize = sizeof( ofn );
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = "Text Files\0*.txt\0All Files\0*.*\0";
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;

    if ( GetOpenFileName( &ofn ) ) {
        FILE* file = fopen( fileName, "rb" );

        if ( !file ) {
            MessageBox( hwnd, "Erro ao abrir o arquivo.", "Erro", MB_OK | MB_ICONERROR );
            return;
        }

        if ( file ) {
            const size_t BUFFER_SIZE = 64 * 1024;
            char buffer[BUFFER_SIZE + 1];
            std::string fileContent;

            while ( !feof( file ) ) {
                size_t bytesRead = fread( buffer, 1, BUFFER_SIZE, file );
                if ( bytesRead > 0 ) {
                    buffer[bytesRead] = '\0';
                    fileContent.append( buffer );
                }
            }

            fclose( file );
            SetWindowText( hEdit, fileContent.c_str() );
            strcpy( currentFilePath, fileName );
            UpdateStatusBar( currentFilePath );
        }
    }
}

/////////////////////////////////////////////////////////////////////////////////
// Função para Salvar Arquivos em Blocos                                       //
/////////////////////////////////////////////////////////////////////////////////
void SaveFileLarge( HWND hwnd ) {
    OPENFILENAME ofn;
    char fileName[MAX_PATH] = "";

    if ( strlen( currentFilePath ) > 0 ) {
        strcpy( fileName, currentFilePath );
    } else {
        strcpy( fileName, "novo_arquivo.txt" );
    }

    ZeroMemory( &ofn, sizeof( ofn ) );
    ofn.lStructSize = sizeof( ofn );
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = "Text Files\0*.txt\0All Files\0*.*\0";
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_OVERWRITEPROMPT;

    if ( GetSaveFileName( &ofn ) ) {
        int length = GetWindowTextLength( hEdit );
        const size_t BUFFER_SIZE = 64 * 1024;
        char* buffer = ( char* )malloc( BUFFER_SIZE + 1 );

        FILE* file = fopen( fileName, "wb" );
        if ( file ) {
            for ( int i = 0; i < length; i += BUFFER_SIZE ) {
                int chunkSize = std::min(static_cast<int>(BUFFER_SIZE), length - i);
                GetWindowText( hEdit, buffer, chunkSize + 1 );
                fwrite( buffer, 1, chunkSize, file );
            }
            fclose( file );
        }

        free( buffer );
        strcpy( currentFilePath, fileName );
        UpdateStatusBar( currentFilePath );
    }
}

/////////////////////////////////////////////////////////////////////////////////
// Callback para o controle de edição (Captura teclas direcionais)             //
/////////////////////////////////////////////////////////////////////////////////
LRESULT CALLBACK EditProc( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam ) {
    static WNDPROC oldProc = ( WNDPROC )GetWindowLongPtr( hwnd, GWLP_USERDATA );

    switch ( uMsg ) {
    case WM_KEYUP: // Captura teclas como Up, Down, Left, Right
        //std::cout << "WM_KEYUP no controle de edição: Tecla pressionada." << std::endl; // Depuração
        UpdateCursorPos( hwnd );
        break;
    }

    return CallWindowProc( oldProc, hwnd, uMsg, wParam, lParam );
}

/////////////////////////////////////////////////////////////////////////////////
// Callback para processar eventos da janela principal                         //
/////////////////////////////////////////////////////////////////////////////////
LRESULT CALLBACK WindowProc( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam ) {
    switch ( uMsg ) {
    case WM_CREATE: {
        hEdit = CreateWindow(
                    "EDIT", "",
                    WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL,
                    0, 0, 0, 0,
                    hwnd, ( HMENU ) ID_EDIT_TEXT, NULL, NULL
                );

        // Substitui o procedimento padrão do controle de edição
        WNDPROC oldProc = ( WNDPROC )SetWindowLongPtr( hEdit, GWLP_WNDPROC, ( LONG_PTR )EditProc );
        SetWindowLongPtr( hEdit, GWLP_USERDATA, ( LONG_PTR )oldProc );

        hStatus = CreateWindowEx(
                      0, STATUSCLASSNAME, NULL,
                      WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
                      0, 0, 0, 0,
                      hwnd, NULL, GetModuleHandle( NULL ), NULL
                  );

        UpdateStatusBar( "Pronto" );
    }
    break;

    case WM_SIZE: {
        RECT rect;
        GetClientRect( hwnd, &rect );

        int statusHeight = 20; // Altura da barra de status
        int editHeight = rect.bottom - statusHeight;

        MoveWindow( hEdit, 0, 0, rect.right, editHeight, TRUE );
        MoveWindow( hStatus, 0, editHeight, rect.right, statusHeight, TRUE );
    }
    break;

    case WM_COMMAND:
        switch ( LOWORD( wParam ) ) {
        case ID_FILE_NEW:
            SetWindowText( hEdit, "" );
            strcpy( currentFilePath, "" );
            UpdateStatusBar( "Novo arquivo" );
            break;

        case ID_FILE_OPEN:
            OpenFileLarge( hwnd );
            break;

        case ID_FILE_SAVE:
            SaveFileLarge( hwnd );
            break;

        case ID_FILE_EXIT:
            PostQuitMessage( 0 );
            break;

        case ID_FIND:
            FindText( hwnd );
            break;

        case ID_REPLACE:
            ReplaceText( hwnd );
            break;
        }
        break;

    case WM_DESTROY:
        PostQuitMessage( 0 );
        break;

    default:
        return DefWindowProc( hwnd, uMsg, wParam, lParam );
    }
    return 0;
}

/////////////////////////////////////////////////////////////////////////////////
// Função principal                                                            //
/////////////////////////////////////////////////////////////////////////////////
int WINAPI WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow ) {
    const char CLASS_NAME[] = "EditorSimples";

    WNDCLASSEX wc = {0};
    wc.cbSize = sizeof( WNDCLASSEX );
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;

    wc.hIcon = LoadIcon( hInstance, MAKEINTRESOURCE( IDI_APP_ICON ) );
    wc.hIconSm = ( HICON )LoadImage( hInstance, MAKEINTRESOURCE( IDI_APP_ICON ), IMAGE_ICON, 16, 16, 0 );

    wc.hCursor = LoadCursor( NULL, IDC_ARROW );
    wc.hbrBackground = ( HBRUSH )( COLOR_WINDOW + 1 );

    RegisterClassEx( &wc );

    HMENU hMenu = CreateMenu();  // <-- Cria menu Arquivo
    HMENU hFileMenu = CreateMenu();
    AppendMenu( hFileMenu, MF_STRING, ID_FILE_OPEN, "Abrir\tCtrl-O" );
    AppendMenu( hFileMenu, MF_STRING, ID_FILE_SAVE, "Salvar\tCtrl-S" );
    AppendMenu( hFileMenu, MF_STRING, ID_FILE_NEW, "Novo\tCtrl-N" );
    AppendMenu( hFileMenu, MF_STRING, ID_FILE_EXIT, "Sair\tCtrl-Q" );
    AppendMenu( hMenu, MF_POPUP, ( UINT_PTR )hFileMenu, "Arquivo" );

    HMENU hEditMenu = CreateMenu(); // <-- Cria menu Editar
    AppendMenu( hEditMenu, MF_STRING, ID_FIND, "Procurar\tCtrl-F" );
    AppendMenu( hEditMenu, MF_STRING, ID_REPLACE, "Substituir\tCtrl-H" );
    AppendMenu( hMenu, MF_POPUP, ( UINT_PTR )hEditMenu, "Editar" );



    ACCEL accTable[] = {
        { FCONTROL | FVIRTKEY, 'O', ID_FILE_OPEN },
        { FCONTROL | FVIRTKEY, 'S', ID_FILE_SAVE },
        { FCONTROL | FVIRTKEY, 'N', ID_FILE_NEW },
        { FCONTROL | FVIRTKEY, 'Q', ID_FILE_EXIT },
        { FCONTROL | FVIRTKEY, 'F', ID_FIND },
        { FCONTROL | FVIRTKEY, 'H', ID_REPLACE }
    };
    HACCEL hAccel = CreateAcceleratorTable( accTable, 6 );

    HWND hwnd = CreateWindowEx(
                    0, CLASS_NAME, "Editor de Texto Simples",
                    WS_OVERLAPPEDWINDOW,
                    CW_USEDEFAULT, CW_USEDEFAULT, 800, 600,
                    NULL, hMenu, hInstance, NULL
                );

    ShowWindow( hwnd, nCmdShow );

    MSG msg = {0};
    while ( GetMessage( &msg, NULL, 0, 0 ) ) {
        if ( !TranslateAccelerator( hwnd, hAccel, &msg ) ) {
            TranslateMessage( &msg );
            DispatchMessage( &msg );
        }
    }

    return 0;
}
