# Mini Sistema de Arquivos em Memória

Simulador de sistema de arquivos Unix-like implementado em C++. Todas as operações ocorrem **exclusivamente em memória RAM** — nenhuma chamada ao sistema de arquivos real do sistema operacional é feita.

---

## 1. Como Compilar e Executar

### Pré-requisitos
- Compilador C++ com suporte a C++11 ou superior (g++ recomendado)

### Compilação
```bash
g++ -o sistema_arquivos main.cpp
```

### Execução
```bash
./sistema_arquivos
```
No Windows:
```bash
sistema_arquivos.exe
```

### Exemplo de sessão
```
/> touch relatorio.txt
/> echo "Conteudo do relatorio" > relatorio.txt
Conteúdo gravado em 'relatorio.txt' (22 bytes)
/> cat relatorio.txt
cat: Conteudo do relatorio
/> ls
[TEXT] relatorio.txt
```

---

## 2. Estruturas de Dados

### 2.1 File Control Block — `struct File`

```cpp
struct File {
    string nome;       // Nome do arquivo (ex: "relatorio.txt")
    int    tamanho;    // Tamanho em bytes do conteúdo atual
    int    id;         // ID único — simula o número de inode
    int    owner_id;   // UID do proprietário
    int    group_id;   // GID do grupo dono do arquivo
    string conteudo;   // Conteúdo do arquivo (armazenado em memória)
    Type   tipo;       // Enum: TEXT, NUMERIC, BINARY, PROGRAM
    time_t criado;     // Timestamp de criação
    time_t modificado; // Timestamp da última modificação
    time_t acesso;     // Timestamp do último acesso
    int    permissoes; // Bitmask de permissões no estilo Unix (ex: 0644)
};
```

**Conexão teórica:** Esta struct é a implementação direta do **File Control Block (FCB)**, também chamado de *inode* em sistemas Unix. O FCB é a estrutura central de um sistema de arquivos — ele armazena todos os metadados sobre um arquivo, separando a *descrição* do arquivo (metadados) do seu *conteúdo* (dados). Em sistemas reais como o ext4, o inode contém os mesmos campos: tamanho, timestamps (atime/mtime/ctime), UID do dono, permissões e ponteiros para os blocos de dados no disco.

O campo `id` simula o número de inode: é gerado pelo contador global `proximoIdArquivo`, que incrementa a cada novo arquivo criado, garantindo unicidade — exatamente como o kernel Linux aloca inodes sequencialmente em partições formatadas.

### 2.2 Nó da Árvore de Diretórios — `struct Directory`

```cpp
struct Directory {
    string             nome;           // Nome do diretório
    Directory*         pai;            // Ponteiro ao diretório pai (nullptr = raiz)
    vector<Directory*>* subDiretorios; // Filhos (subdiretórios)
    vector<File*>       arquivos;      // Arquivos contidos neste diretório
};
```

**Conexão teórica:** O `Directory` implementa um **nó de uma árvore N-ária** — a estrutura de dados que todos os sistemas de arquivos modernos usam para organizar hierarquicamente seus conteúdos.

- O ponteiro `pai` permite navegação para cima (`cd ..`) em tempo O(1)
- O vetor `subDiretorios` permite navegação para baixo (filhos)
- Cada `Directory` é um nó da árvore; a raiz (`/`) é o único nó sem pai (`pai == nullptr`)

### 2.3 Enum de Tipos — `enum Type`

```cpp
enum Type { TEXT, NUMERIC, BINARY, PROGRAM };
```

Detectado automaticamente pela extensão do nome do arquivo na função `touch`:

| Extensão | Tipo    | Analogia real             |
|----------|---------|---------------------------|
| `.txt`   | TEXT    | Arquivo de texto puro     |
| `.csv`   | NUMERIC | Dados estruturados/tabela |
| `.exe`   | PROGRAM | Executável                |
| outros   | BINARY  | Dados binários genéricos  |

---

## 3. Vantagens da Estrutura em Árvore

A estrutura de diretórios em árvore é superior a alternativas mais simples (como lista plana) pelas seguintes razões:

| Aspecto | Lista Plana | Árvore (nossa implementação) |
|---------|-------------|------------------------------|
| Organização | Todos os arquivos no mesmo nível | Hierarquia lógica de pastas |
| Navegação | Busca linear em todos os arquivos | Cada nó só busca em seus filhos diretos |
| Escalabilidade | Degradação com muitos arquivos | Desempenho estável com subárvores menores |
| Caminhos absolutos | Impossível sem hierarquia | Possível: `/home/usuario/documentos/` |
| Isolamento | Conflito de nomes entre todos os arquivos | Nomes duplicados válidos em diretórios diferentes |
| Analogia humana | Caixas de sapato com tudo misturado | Pastas etiquetadas dentro de gavetas |

No código, a navegação aproveita diretamente os ponteiros da árvore:

```cpp
// Subir: O(1) via ponteiro pai
diretorioAtual = diretorioAtual->pai;

// Descer: busca linear apenas nos filhos diretos do nó atual
for (int i = 0; i < subDiretorios.size(); ++i) {
    if (subDiretorios[i]->nome == nomeDiretorio) {
        diretorioAtual = subDiretorios[i];
    }
}
```

---

## 4. Conceito de Arquivo e Seus Atributos

Um arquivo é, do ponto de vista do SO, uma **abstração sobre uma sequência de bytes**, identificada por um nome e descrita por metadados. Os atributos implementados no `struct File` correspondem diretamente aos atributos padrão definidos pela teoria de sistemas operacionais:

| Atributo no código | Campo teórico       | Descrição                                      |
|--------------------|---------------------|------------------------------------------------|
| `id`               | Número de inode     | Identificador único persistente do arquivo     |
| `nome`             | Nome                | Apelido humano no diretório (ligado ao inode)  |
| `tamanho`          | Tamanho             | Número de bytes do conteúdo                    |
| `tipo`             | Tipo/formato        | Categoria semântica do conteúdo                |
| `owner_id`         | UID do proprietário | Usuário que criou e "possui" o arquivo         |
| `group_id`         | GID do grupo        | Grupo dono do arquivo (padrão: mesmo que owner)|
| `permissoes`       | Modo de acesso      | Bitmask rwx para owner, group e others         |
| `criado`           | ctime               | Quando o inode foi criado                      |
| `modificado`       | mtime               | Última vez que o conteúdo foi alterado         |
| `acesso`           | atime               | Última vez que o conteúdo foi lido             |
| `conteudo`         | Blocos de dados     | Os dados em si (simplificado: string em RAM)   |

---

## 5. Operações com Arquivos

### 5.1 Criação — `touch`

Aloca um novo `File*` na heap, inicializa todos os campos do FCB e incrementa o contador global de inodes. O tipo é inferido pela extensão do nome via `endsWith()`.

Conexão teórica: no Linux real, `open(path, O_CREAT)` aloca um novo inode na tabela de inodes do filesystem e cria uma entrada no diretório apontando para ele. Aqui fazemos o equivalente: alocamos o FCB e o adicionamos ao vetor de arquivos do diretório atual.

### 5.2 Escrita — `echo`

Suporta dois modos:
- `echo "texto" > arquivo` — sobrescreve o conteúdo (`conteudo = novoTexto`)
- `echo "texto" >> arquivo` — concatena ao final (`conteudo += novoTexto`)

Atualiza `tamanho` e `modificado` após cada escrita. Verifica permissão `'w'` antes de qualquer modificação.

### 5.3 Leitura — `cat`

Busca o arquivo por nome, verifica permissão `'r'` e exibe `conteudo`. Atualiza `acesso` com `time(nullptr)` após leitura bem-sucedida, replicando o comportamento do `atime` Unix.

### 5.4 Cópia — `cp`

Aloca um **novo** `File*` com novo `id` (novo inode), copia `conteudo` e `tamanho` da origem. O novo arquivo pertence ao usuário atual (`owner_id = usuarioAtualId`) e recebe permissões padrão `0644`. O tipo é re-detectado pelo nome do destino. Requer permissão `'r'` na origem.

### 5.5 Renomeação — `mv`

Muta o `File*` existente: apenas altera `nome` e atualiza `modificado`. O `id` (inode), `conteudo`, `owner_id` e demais campos permanecem intactos — exatamente como `rename()` no Linux, que não realoca o inode, apenas atualiza a entrada no diretório. O tipo é re-detectado pela nova extensão. Requer permissão `'w'` no arquivo.

### 5.6 Exclusão — `rm`

Remove a entrada do vetor com `erase()` e libera a memória do heap com `delete`. Requer permissão `'w'`.

Conexão teórica: em sistemas reais, `unlink()` decrementa o contador de hard links do inode. Quando chega a zero, o inode é liberado. Aqui simplificamos: há exatamente um "link" por arquivo, então a remoção é direta.

---

## 6. FCB e Inode Simulado

O **inode** (index node) é a estrutura fundamental que separa o *nome* do arquivo de seus *dados e metadados*. No nosso sistema:

```
Diretório atual:
  arquivos[0] → File* {id=0, nome="doc.txt", tamanho=15, owner_id=0, ...}
  arquivos[1] → File* {id=1, nome="dados.csv", tamanho=42, owner_id=1, ...}
```

O campo `id` age como número de inode:
- Gerado pelo contador global `proximoIdArquivo` (começa em 0)
- Único por arquivo, nunca reutilizado na sessão
- Não depende do nome (o mesmo inode sobrevive a um `mv`)

Em sistemas reais (ext4, NTFS), o inode e o nome do arquivo são entidades separadas: o diretório guarda o par `(nome → número_de_inode)` e os metadados ficam na tabela de inodes. Aqui comprimimos isso: o `File*` acumula ambos os papéis, mas o campo `id` preserva a semântica de identificador único independente do nome.

---

## 7. Sistema de Permissões RWX com Bitmasks

### 7.1 Modelo de Bits

O campo `permissoes` é um inteiro cujos bits codificam as permissões no modelo Unix:

```
Octal: 0 6 4 4
        │ │ │ └── others: 100 = r-- (somente leitura para todos os outros)
        │ │ └──── group:  100 = r-- (somente leitura para o grupo)
        │ └────── owner:  110 = rw- (leitura e escrita para o dono)
        └──────── sticky/setuid (não usado nesta implementação)
```

Cada dígito octal representa 3 bits (rwx):

| Valor octal | Binário | Permissão |
|-------------|---------|-----------|
| 7           | 111     | rwx       |
| 6           | 110     | rw-       |
| 5           | 101     | r-x       |
| 4           | 100     | r--       |
| 0           | 000     | ---       |

### 7.2 Verificação com AND Binário

A função `hasPermission()` usa o operador `&` (AND bit a bit) para testar um bit específico:

```cpp
bool hasPermission(File* arquivo, int usuarioId, char operacao) {
    if (usuarioId == 0) return true;    // root: acesso total irrestrito

    int mascara = 0;
    if (usuarioId == arquivo->owner_id) {
        // bits de owner: r=0400 w=0200 x=0100
        if (operacao == 'r') mascara = 0400; // 100 000 000
        if (operacao == 'w') mascara = 0200; // 010 000 000
        if (operacao == 'x') mascara = 0100; // 001 000 000
    } else if (usuarioGrupoId == arquivo->group_id) {
        // bits de grupo: r=0040 w=0020 x=0010
        if (operacao == 'r') mascara = 0040; // 000 100 000
        if (operacao == 'w') mascara = 0020; // 000 010 000
        if (operacao == 'x') mascara = 0010; // 000 001 000
    } else {
        // bits de others: r=0004 w=0002 x=0001
        if (operacao == 'r') mascara = 0004; // 000 000 100
        if (operacao == 'w') mascara = 0002; // 000 000 010
        if (operacao == 'x') mascara = 0001; // 000 000 001
    }

    return (arquivo->permissoes & mascara) != 0;
}
```

**Exemplo prático com `0644`:**
```
permissoes = 0644 = 110 100 100

Teste: owner pode ler? (mascara = 0400 = 100 000 000)
  0644 & 0400 = 110 100 100
              & 100 000 000
              = 100 000 000 ≠ 0  → PERMITIDO ✅

Teste: others podem escrever? (mascara = 0002 = 000 000 010)
  0644 & 0002 = 110 100 100
              & 000 000 010
              = 000 000 000 = 0  → NEGADO ❌
```

### 7.3 Alteração com `chmod`

Converte a string octal digitada pelo usuário para inteiro usando base 8:

```cpp
int novaPermissao = stoi("755", nullptr, 8);
// 7*64 + 5*8 + 5*1 = 493 decimal = 111 101 101 binário = rwxr-xr-x
```

Somente o proprietário do arquivo ou o root (id=0) podem alterar permissões — replicando a regra Unix onde apenas o owner ou superusuário pode fazer `chmod`.

### 7.4 Verificação em Todas as Operações

| Operação | Permissão verificada |
|----------|----------------------|
| `cat`    | `'r'` (leitura)      |
| `echo`   | `'w'` (escrita)      |
| `cp`     | `'r'` na origem      |
| `mv`     | `'w'` no arquivo     |
| `rm`     | `'w'` no arquivo     |
| `chmod`  | owner_id ou root     |
| `stat`   | nenhuma (apenas exibe metadados) |

---

## 8. Simulação de Alocação de Blocos (Discussão Teórica)

Esta implementação armazena o conteúdo dos arquivos diretamente em `string conteudo` na RAM, o que é uma simplificação proposital para focar nos conceitos de FCB, árvore e permissões. Em um sistema de arquivos real, o armazenamento funciona de forma muito diferente:

### 8.1 Disco como Array de Blocos

O disco é dividido em **blocos de tamanho fixo** (tipicamente 4 KB no ext4):

```
disco[0..4095]    → bloco 0
disco[4096..8191] → bloco 1
disco[8192..12287] → bloco 2
...
```

Uma implementação simulada poderia ser:

```cpp
const int TAM_BLOCO = 512;
const int NUM_BLOCOS = 1024;
char disco[TAM_BLOCO * NUM_BLOCOS]; // "disco" em memória
bool blocoLivre[NUM_BLOCOS];        // mapa de blocos livres
```

### 8.2 Métodos de Alocação

**Alocação Contígua:** o arquivo ocupa blocos consecutivos. O FCB armazena bloco inicial e tamanho.
- Vantagem: acesso sequencial rápido
- Desvantagem: fragmentação externa, expansão difícil

**Alocação Indexada (estilo ext4):** o inode contém array de ponteiros diretos para blocos e ponteiros indiretos para tabelas de ponteiros.
- Vantagem: sem fragmentação externa, expansão simples
- Desvantagem: overhead dos blocos de índice

**Alocação Encadeada (FAT):** cada bloco contém ponteiro para o próximo bloco do arquivo.
- Vantagem: sem fragmentação externa
- Desvantagem: acesso aleatório lento

### 8.3 Relação com o FCB

Numa implementação com blocos, o `struct File` teria ponteiros para os blocos em vez do campo `conteudo`:

```cpp
struct File {
    // ... campos de metadados ...
    int blocos[12];      // ponteiros diretos para até 12 blocos
    int bloco_indireto;  // ponteiro para tabela de ponteiros adicionais
};
```

A leitura exigiria montar o conteúdo iterando pelos blocos referenciados — em vez de simplesmente acessar `arquivo->conteudo`.

---

## 9. Comandos Disponíveis

| Comando | Sintaxe | Descrição |
|---------|---------|-----------|
| `mkdir` | `mkdir <nome>` | Cria subdiretório no diretório atual |
| `cd`    | `cd <nome>` / `cd ..` | Navega para subdiretório ou volta ao pai |
| `ls`    | `ls` | Lista subdiretórios e arquivos do diretório atual |
| `touch` | `touch <nome>` | Cria arquivo vazio com FCB completo |
| `echo`  | `echo "texto" > arquivo` | Sobrescreve conteúdo do arquivo |
| `echo`  | `echo "texto" >> arquivo` | Concatena ao conteúdo do arquivo |
| `cat`   | `cat <arquivo>` | Exibe conteúdo do arquivo |
| `cp`    | `cp <origem> <destino>` | Copia arquivo (novo inode) |
| `mv`    | `mv <origem> <destino>` | Renomeia arquivo (mesmo inode) |
| `rm`    | `rm <arquivo>` | Remove arquivo e libera memória |
| `chmod` | `chmod <octal> <arquivo>` | Altera permissões (ex: `chmod 755 prog.exe`) |
| `stat`  | `stat <arquivo>` | Exibe todos os metadados do FCB (inode, tipo, tamanho, permissões, timestamps) |
| `su`    | `su <id>` | Muda o usuário e o grupo atual (ex: `su 0` = root) |
| `exit`  | `exit` | Encerra o simulador liberando toda a memória |

---

## 10. Exemplos de Uso Comparados com Linux Real

### Criar arquivo
```
Nosso sistema:    /> touch nota.txt
Linux real:       $ touch nota.txt

Diferença: nosso touch inicializa todos os campos do FCB em RAM;
o Linux cria uma entrada de inode no filesystem em disco.
```

### Escrever e ler conteúdo
```
Nosso sistema:
  /> echo "Ola mundo" > nota.txt
  Conteúdo gravado em 'nota.txt' (9 bytes)
  /> cat nota.txt
  cat: Ola mundo

Linux real:
  $ echo "Ola mundo" > nota.txt
  $ cat nota.txt
  Ola mundo

Diferença: nosso echo faz parse manual da string e atribui ao campo conteudo;
o Linux usa chamadas de sistema write() que gravam bytes em blocos de disco.
```

### Criar hierarquia de diretórios
```
Nosso sistema:
  /> mkdir documentos
  /> cd documentos
  /documentos> touch relat.txt
  /documentos> cd ..
  /> ls
  [DIR]    documentos
  [TEXT] relat.txt        ← arquivo criado ANTES do cd ficou na raiz

Linux real:
  $ mkdir documentos && cd documentos
  $ touch relat.txt && cd ..
  $ ls
  documentos/   relat.txt

Mesma hierarquia em árvore; diferença é que o Linux persiste em disco.
```

### Copiar vs renomear (diferença de inode)
```
Nosso sistema:
  /> touch original.txt
  /> cp original.txt copia.txt     ← id=0 original, id=1 copia (novo inode)
  /> mv copia.txt renomeado.txt    ← id=1 preservado (mesmo inode, novo nome)

Linux real:
  $ touch original.txt
  $ cp original.txt copia.txt      ← novo inode alocado
  $ mv copia.txt renomeado.txt     ← rename() não altera inode
  $ ls -i
  12345 original.txt   12346 renomeado.txt   ← inodes distintos

Comportamento idêntico: cp cria inode novo, mv preserva o mesmo.
```

### Controle de acesso com permissões
```
Nosso sistema:
  /> touch segredo.txt
  /> echo "sigiloso" > segredo.txt
  /> chmod 600 segredo.txt
  /> su 1
  /> cat segredo.txt
  cat: você não possui permissão.
  /> su 0
  /> cat segredo.txt
  cat: sigiloso

Linux real:
  $ touch segredo.txt && echo "sigiloso" > segredo.txt
  $ chmod 600 segredo.txt
  $ su usuario1 -c "cat segredo.txt"
  cat: segredo.txt: Permission denied
  $ sudo cat segredo.txt
  sigiloso

Mecanismo idêntico: AND bit a bit entre permissoes (0600 = 110 000 000)
e máscara others-read (0004 = 000 000 100) resulta em 0 → acesso negado.
```

### Remover arquivo
```
Nosso sistema:
  /> rm nota.txt
  rm: arquivo 'nota.txt' removido.
  /> rm nota.txt
  rm: arquivo 'nota.txt' não encontrado.

Linux real:
  $ rm nota.txt
  $ rm nota.txt
  rm: cannot remove 'nota.txt': No such file or directory

Diferença: nosso rm chama erase() no vetor + delete no heap;
o Linux chama unlink(), que libera o inode quando o link count chega a zero.
```

---

## 11. Arquitetura do Código

```
main.cpp
├── Tipos e estruturas
│   ├── enum Type           → tipos de arquivo
│   ├── struct File         → FCB (File Control Block / inode simulado)
│   └── struct Directory    → nó da árvore de diretórios
│
├── Funções utilitárias
│   ├── typeToString()      → converte enum para string (usado no ls)
│   ├── endsWith()          → detecta extensão do nome do arquivo
│   └── hasPermission()     → verifica bitmask de permissões com AND bit a bit
│
├── Operações de diretório
│   ├── mkdir()             → aloca e inicializa nó da árvore
│   ├── cd()                → navega pelos ponteiros pai/filho
│   └── ls()                → itera vetores do nó atual
│
├── Operações de arquivo
│   ├── touch()             → aloca File* com FCB completo + inode único
│   ├── echo()              → escreve (>) ou concatena (>>) conteúdo
│   ├── cat()               → lê e exibe conteúdo, atualiza atime
│   ├── cp()                → aloca novo File* (novo inode)
│   ├── mv()                → muta nome no File* existente (mesmo inode)
│   └── rm()                → erase() no vetor + delete no heap
│
├── Controle de acesso
│   ├── chmod()             → altera bitmask via stoi(octal, nullptr, 8)
│   └── su()                → altera usuário atual da sessão
│
└── main()                  → loop do shell interativo
    └── parse + dispatch    → divide linha em comando/argumentos, chama função
```
