#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <sstream>
#include <ctime>
using namespace std;

int proximoIdArquivo = 0;

enum Type {
    TEXT,
    NUMERIC,
    BINARY,
    PROGRAM
};

struct File {
    string nome;
    int tamanho;
    int id;
    int owner_id;
    string conteudo;
    Type tipo;
    time_t criado;
    time_t modificado;
    time_t acesso;
    int permissoes;
};

struct Directory {
    string nome;
    Directory* pai;
    vector<Directory*> *subDiretorios;
    vector<File*> arquivos;
};

string typeToString(Type tipo) {
    switch (tipo) {
        case TEXT:    return "TEXT";
        case NUMERIC: return "NUMERIC";
        case BINARY:  return "BINARY";
        case PROGRAM: return "PROGRAM";
        default:      return "UNKNOWN";
    }
}

bool endsWith(const string& texto, const string& sufixo){
    if (texto.length() < sufixo.length()){
        return false;
    }

    return texto.compare(texto.length() - sufixo.length(), sufixo.length(), sufixo) == 0;
}

bool hasPermission(File* arquivo, int usuarioId, char operacao) {
    if (usuarioId == 0) {
        return true; //Uso liberado
    }

    int mascara = 0;

    if (usuarioId == arquivo->owner_id) {
        if (operacao == 'r') mascara = 0400; 
        if (operacao == 'w') mascara = 0200; 
        if (operacao == 'x') mascara = 0100; 
    } 
    else {
        if (operacao == 'r') mascara = 0004; 
        if (operacao == 'w') mascara = 0002; 
        if (operacao == 'x') mascara = 0001; 
    }

    return (arquivo->permissoes & mascara) != 0;
}

Directory* mkdir(string nome, Directory* pai = nullptr){
    Directory* novoDiretorio = new Directory;
    novoDiretorio->nome = nome;
    novoDiretorio->pai = pai;
    novoDiretorio->subDiretorios = new vector<Directory*>;
    return novoDiretorio;
}

void cd(Directory*& diretorioAtual, string nomeDiretorio){
    if (nomeDiretorio == "..") {
        if (diretorioAtual->pai != nullptr) {
            diretorioAtual = diretorioAtual->pai;
        }
        return;
    }

    vector<Directory*>& subDiretorios = *(diretorioAtual->subDiretorios);

    for (int i = 0; i < subDiretorios.size(); ++i) {
        if (subDiretorios[i]->nome == nomeDiretorio) {
            diretorioAtual = subDiretorios[i];
            return;
        }
    }
    
    cout << "Diretório '" << nomeDiretorio << "' não encontrado." << endl;
}

void ls(Directory* diretorioAtual){
    vector<Directory*>& subDiretorios = *(diretorioAtual->subDiretorios);
    for(int i = 0; i<subDiretorios.size(); i++){
        cout << "[DIR]    " << subDiretorios[i]->nome <<endl; 
    }

    vector<File*>& arquivos = diretorioAtual->arquivos;
    for(int i=0; i<arquivos.size(); i++){
        cout << "[" << typeToString(arquivos[i]->tipo) << "] " << arquivos[i]->nome << endl;
    }
}

File* touch(string nomeArquivo, int usuarioAtualId){
    File* novoArquivo = new File;

    novoArquivo->nome=nomeArquivo;
    novoArquivo->conteudo="";
    novoArquivo->tamanho=0;
    novoArquivo->id=proximoIdArquivo;
    novoArquivo->owner_id=usuarioAtualId;
    novoArquivo->criado = time(nullptr);
    novoArquivo->modificado = time(nullptr);
    novoArquivo->acesso = time(nullptr);
    novoArquivo->permissoes = 0644; 

    if (endsWith(nomeArquivo, ".txt")) {
        novoArquivo->tipo = TEXT;
    } else if (endsWith(nomeArquivo, ".csv")) {
        novoArquivo->tipo = NUMERIC;
    } else if (endsWith(nomeArquivo, ".exe")) {
        novoArquivo->tipo = PROGRAM;
    } else {
        novoArquivo->tipo = BINARY;
    }

    proximoIdArquivo+=1;

    return novoArquivo;
}

void echo(string argumentos, Directory* diretorioAtual, int usuarioAtualId){
    int pos = argumentos.find_last_of('>');
    if (pos == string::npos) {
        cout << "Sintaxe inválida." << endl;
        return;
    }

    string operacao;
    if (pos > 0 && argumentos[pos - 1] == '>') {
        operacao = ">>";
    } else {
        operacao = ">";
    }

    string conteudo = argumentos.substr(0, pos - (operacao.size() - 1));

    while (!conteudo.empty() && conteudo.back() == ' '){
        conteudo.pop_back();
    }

    if (conteudo.size() >= 2 && conteudo.front() == '"' && conteudo.back() == '"') {
        conteudo = conteudo.substr(1, conteudo.size() - 2);
    }

    string arquivo = argumentos.substr(pos + 1);
    while (!arquivo.empty() && arquivo.front() == ' '){
        arquivo.erase(0, 1);
    }

    vector<File*>& arquivos = diretorioAtual->arquivos;
    for (int i = 0; i < arquivos.size(); ++i) {
        if (arquivos[i]->nome == arquivo) {
            if (!hasPermission(arquivos[i], usuarioAtualId, 'w')) {
                cout << "echo: permissão negada para escrever em '" << arquivo << "'" << endl;
                return;
            }

            if (operacao == ">") { //Substitui
                arquivos[i]->conteudo = conteudo;
            } else { //Adiciona
                arquivos[i]->conteudo += conteudo;
            }
            arquivos[i]->tamanho = arquivos[i]->conteudo.size();
            arquivos[i]->modificado = time(nullptr);

            cout << "Conteúdo gravado em '" << arquivo << "' (" << arquivos[i]->tamanho << " bytes)" << endl;
            return;
        }
    }

    cout << "echo: arquivo/diretório '" << arquivo << "' não encontrado." << endl;
}

void cat(string arquivo, Directory* diretorioAtual, int usuarioAtualId){
    if(arquivo.empty()){
        cout << "cat: falta o nome do arquivo." << endl;
        return;
    }

    vector<File*>& arquivos = diretorioAtual->arquivos;
    for(int i=0; i<arquivos.size(); i++){
        if(arquivo==arquivos[i]->nome){
            if(hasPermission (arquivos[i], usuarioAtualId, 'r')){
                cout << "cat: " << arquivos[i]->conteudo <<endl;
                arquivos[i]->acesso = time(nullptr);
            } else {
                cout << "cat: você não possui permissão." << endl;
            }
            return;
        }
    }
    cout << "cat: arquivo '" << arquivo << "' não encontrado." << endl;
}

void cp(string argumentos, Directory* diretorioAtual, int usuarioAtualId) {
    int posEspaco = argumentos.find(' ');
    if (posEspaco == string::npos) {
        cout << "cp: uso: cp <origem> <destino>" << endl;
        return;
    }

    string origem = argumentos.substr(0, posEspaco);
    string destino = argumentos.substr(posEspaco + 1);

    vector<File*>& arquivos = diretorioAtual->arquivos;

    File* arquivoOrigem = nullptr;
    for (int i = 0; i < arquivos.size(); ++i) {
        if (arquivos[i]->nome == origem) {
            arquivoOrigem = arquivos[i];
            break;
        }
    }

    if (arquivoOrigem == nullptr) {
        cout << "cp: arquivo '" << origem << "' não encontrado." << endl;
        return;
    }

    if (!hasPermission(arquivoOrigem, usuarioAtualId, 'r')) {
        cout << "cp: permissão negada para ler '" << origem << "'" << endl;
        return;
    }

    for (int i = 0; i < arquivos.size(); ++i) {
        if (arquivos[i]->nome == destino) {
            cout << "cp: arquivo '" << destino << "' já existe." << endl;
            return;
        }
    }

    File* novoArquivo = new File;
    novoArquivo->nome = destino;
    novoArquivo->conteudo = arquivoOrigem->conteudo;
    novoArquivo->tamanho = arquivoOrigem->tamanho;
    novoArquivo->id = proximoIdArquivo;
    novoArquivo->owner_id = usuarioAtualId;
    novoArquivo->criado = time(nullptr);
    novoArquivo->modificado = time(nullptr);
    novoArquivo->acesso = time(nullptr);
    novoArquivo->permissoes = 0644;

    if (endsWith(destino, ".txt")) {
        novoArquivo->tipo = TEXT;
    } else if (endsWith(destino, ".csv")) {
        novoArquivo->tipo = NUMERIC;
    } else if (endsWith(destino, ".exe")) {
        novoArquivo->tipo = PROGRAM;
    } else {
        novoArquivo->tipo = BINARY;
    }

    proximoIdArquivo++;
    arquivos.push_back(novoArquivo);
    cout << "cp: '" << origem << "' copiado para '" << destino << "'" << endl;
}

void mv(string argumentos, Directory* diretorioAtual, int usuarioAtualId) {
    int posEspaco = argumentos.find(' ');
    if (posEspaco == string::npos) {
        cout << "mv: uso: mv <origem> <destino>" << endl;
        return;
    }

    string origem = argumentos.substr(0, posEspaco);
    string destino = argumentos.substr(posEspaco + 1);

    vector<File*>& arquivos = diretorioAtual->arquivos;

    File* arquivoOrigem = nullptr;
    for (int i = 0; i < arquivos.size(); ++i) {
        if (arquivos[i]->nome == origem) {
            arquivoOrigem = arquivos[i];
            break;
        }
    }

    if (arquivoOrigem == nullptr) {
        cout << "mv: arquivo '" << origem << "' não encontrado." << endl;
        return;
    }

    if (!hasPermission(arquivoOrigem, usuarioAtualId, 'w')) {
        cout << "mv: permissão negada para renomear '" << origem << "'" << endl;
        return;
    }

    for (int i = 0; i < arquivos.size(); ++i) {
        if (arquivos[i]->nome == destino) {
            cout << "mv: arquivo '" << destino << "' já existe." << endl;
            return;
        }
    }

    arquivoOrigem->nome = destino;
    arquivoOrigem->modificado = time(nullptr);

    if (endsWith(destino, ".txt")) {
        arquivoOrigem->tipo = TEXT;
    } else if (endsWith(destino, ".csv")) {
        arquivoOrigem->tipo = NUMERIC;
    } else if (endsWith(destino, ".exe")) {
        arquivoOrigem->tipo = PROGRAM;
    } else {
        arquivoOrigem->tipo = BINARY;
    }

    cout << "mv: '" << origem << "' renomeado para '" << destino << "'" << endl;
}

void rm(string argumentos, Directory* diretorioAtual, int usuarioAtualId) {
    if (argumentos.empty()) {
        cout << "rm: falta o nome do arquivo." << endl;
        return;
    }

    vector<File*>& arquivos = diretorioAtual->arquivos;

    for (int i = 0; i < arquivos.size(); ++i) {
        if (arquivos[i]->nome == argumentos) {
            if (!hasPermission(arquivos[i], usuarioAtualId, 'w')) {
                cout << "rm: permissão negada para remover '" << argumentos << "'" << endl;
                return;
            }

            File* arquivoTemp = arquivos[i];
            arquivos.erase(arquivos.begin() + i);
            delete arquivoTemp;
            cout << "rm: arquivo '" << argumentos << "' removido." << endl;
            return;
        }
    }

    cout << "rm: arquivo '" << argumentos << "' não encontrado." << endl;
}

void chmod(string argumentos, Directory* diretorioAtual, int usuarioAtualId) {
    int posEspaco = argumentos.find(' ');
    if (posEspaco == string::npos) {
        cout << "chmod: uso: chmod <permissao> <arquivo>" << endl;
        return;
    }

    string permissao = argumentos.substr(0, posEspaco);
    string nomeArquivo = argumentos.substr(posEspaco + 1);

    bool permissaoValida = !permissao.empty() && permissao.length() <= 4 &&
                           all_of(permissao.begin(), permissao.end(), ::isdigit);

    if (permissaoValida) {
        for (char c : permissao) {
            if (c > '7') {
                permissaoValida = false;
                break;
            }
        }
    }

    if (!permissaoValida) {
        cout << "chmod: permissão inválida. Use formato octal (ex: 755, 644)" << endl;
        return;
    }

    vector<File*>& arquivos = diretorioAtual->arquivos;

    for (int i = 0; i < arquivos.size(); ++i) {
        if (arquivos[i]->nome == nomeArquivo) {
            if (usuarioAtualId != 0 && usuarioAtualId != arquivos[i]->owner_id) {
                cout << "chmod: permissão negada para alterar '" << nomeArquivo << "'. Apenas o proprietário pode alterar permissões." << endl;
                return;
            }

            int novaPermissao = stoi(permissao, nullptr, 8);
            arquivos[i]->permissoes = novaPermissao;
            arquivos[i]->modificado = time(nullptr);
            cout << "chmod: permissões de '" << nomeArquivo << "' alteradas para " << permissao << endl;
            return;
        }
    }

    cout << "chmod: arquivo '" << nomeArquivo << "' não encontrado." << endl;
}

void su(string argumentos, int& usuarioAtualId){
    bool usuarioValido = !argumentos.empty() && all_of(argumentos.begin(), argumentos.end(), ::isdigit);

    if (!usuarioValido) {
        cout << "su: ID de usuário inválido. Forneça um ID válido." << endl;
        return;
    }

    usuarioAtualId = stoi(argumentos);
    cout << "Usuário alterado para ID: " << usuarioAtualId << endl;
}

int main(){
    Directory *diretorioAtual = mkdir("/");
    int usuarioAtualId = 0;

    while(true){
        cout << diretorioAtual->nome << "> ";

        string linha;
        getline(cin, linha);

        string operacao;
        string argumentos;

        int pos = linha.find(' ');
        if (pos == string::npos) {
            operacao = linha;
        } else {
            operacao = linha.substr(0, pos);
            argumentos = linha.substr(pos + 1);
        }

        if (operacao == "mkdir"){
            diretorioAtual->subDiretorios->push_back(mkdir(argumentos, diretorioAtual));
        } else if (operacao == "cd") {
            cd(diretorioAtual, argumentos);
        } else if (operacao == "touch") {
            diretorioAtual->arquivos.push_back(touch(argumentos, usuarioAtualId));
        } else if (operacao == "ls") {
            ls(diretorioAtual);
        } else if (operacao == "echo") {
            echo(argumentos, diretorioAtual, usuarioAtualId);
        } else if (operacao == "cat") {
            cat(argumentos, diretorioAtual, usuarioAtualId);
        } else if (operacao == "su") {
            su(argumentos, usuarioAtualId);
        } else if (operacao == "cp") {
            cp(argumentos, diretorioAtual, usuarioAtualId);
        } else if (operacao == "mv") {
            mv(argumentos, diretorioAtual, usuarioAtualId);
        } else if (operacao == "rm") {
            rm(argumentos, diretorioAtual, usuarioAtualId);
        } else if (operacao == "chmod") {
            chmod(argumentos, diretorioAtual, usuarioAtualId);
        } else {
            cout << "Comando inválido!" << endl;
        }
    }
}