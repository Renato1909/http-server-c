# c-http-server

![CI](https://github.com/Renato1909/http-server-c/actions/workflows/ci.yml/badge.svg)
![Linguagem](https://img.shields.io/badge/C-11-blue)
![Plataforma](https://img.shields.io/badge/plataforma-Windows-lightgrey)

Servidor **HTTP/1.1 escrito em C puro** sobre a API Winsock2 — sem bibliotecas
externas. Projeto de estudo para entender o protocolo HTTP, sockets TCP e
concorrência na camada mais baixa possível.

## Destaques técnicos

- **Parser HTTP/1.x incremental** escrito à mão (request line + headers),
  com acumulação segura em buffer fixo e rejeição de cabeçalhos gigantes (`431`)
- **Concorrência thread-per-connection** via `CreateThread`, com log protegido
  por `CRITICAL_SECTION`
- **Keep-alive HTTP/1.1** completo: múltiplas requisições por conexão,
  tratamento de bytes excedentes (pipelining) e limite de requisições
- **Serviço de arquivos estáticos** com resolução canônica
  (`GetFullPathName`) + bloqueio de path traversal em duas camadas
- Tabela própria de **MIME types**, respostas `HEAD` sem corpo,
  erros padronizados (`400/404/405/431/500/505`)
- **Timeouts de I/O** por socket (`SO_RCVTIMEO`/`SO_SNDTIMEO`)
- Encerramento gracioso do listener via `SetConsoleCtrlHandler` (Ctrl+C)
- Suíte de testes de integração em PowerShell + CI no GitHub Actions

## Como compilar

Requisitos: [MSYS2/MinGW-w64](https://www.msys2.org/) com GCC e `mingw32-make`.

```powershell
mingw32-make          # gera build\http-server.exe
mingw32-make clean    # remove o diretório build\
```

## Como rodar

> Importante: execute a partir da raiz do projeto — o servidor resolve a pasta
> `public/` relativa ao diretório atual.

```powershell
.\build\http-server.exe        # porta padrão 8080
.\build\http-server.exe 9000   # porta customizada
```

Abra http://localhost:8080 no navegador.

## Como testar

A suíte sobe uma instância na porta 8090 e valida 13 cenários reais via curl:

```powershell
mingw32-make test
# ou direto:
powershell -ExecutionPolicy Bypass -File tests\run-tests.ps1
```

Cobertura: status codes, content-types, HEAD sem corpo, métodos não permitidos,
path traversal (literal e encodado), keep-alive reutilizando conexão e HTTP/1.0.

## Endpoints

| Método | Rota            | Descrição                          |
|--------|-----------------|------------------------------------|
| GET    | `/`             | Página inicial (arquivos estáticos)|
| GET    | `/health`       | Health check — retorna `ok`        |
| GET    | `/*qualquer.*`  | Arquivos de `public/` por MIME     |
| *      | outras          | `405 Method Not Allowed`           |

## Arquitetura

```
main.c ──► server.c ──► accept() loop
              │              │
              │              ├─ CreateThread ──► conn.c (loop keep-alive)
              │                                    │
              ├─ router_init()                     ▼
              │                              request.c (parser)
              ▼                                    │
         log.c (thread-safe)                       ▼
                                              router.c ──► response.c
                                                 │             ▲
                                                 ▼             │
                                             mime.c ───────────┘
                                          public/ (assets)
```

| Módulo       | Responsabilidade                                   |
|--------------|----------------------------------------------------|
| `server.c`   | Ciclo Winsock: WSAStartup → bind → listen → accept |
| `conn.c`     | Loop de conexão, keep-alive, timeouts, access log  |
| `request.c`  | Parser incremental de request line e headers       |
| `router.c`   | Rotas, resolução de arquivos, segurança de caminho |
| `response.c` | Montagem e envio de respostas HTTP                 |
| `mime.c`     | Mapeamento extensão → Content-Type                 |
| `log.c`      | Logging thread-safe com timestamp                  |

## Decisões técnicas

- **Thread-per-connection**: simples e suficiente para estudo; um pool de
  threads ou I/O Completion Ports seriam os próximos passos para escala.
- **Buffer fixo de 16 KB** por requisição: evita alocação dinâmica no hot path;
  cabeçalhos maiores recebem `431`.
- **Sem decodificação de percent-encoding**: mantém a superfície de ataque
  pequena; URLs encodadas simplesmente não resolvem para arquivos.
- **Corpo de requisição ignorado** em GET/HEAD (sem upload): POST/PUT retornam
  `405` e fecham a conexão, eliminando problemas de enquadramento.

## Limitações conhecidas (por design)

- Somente Windows (Winsock2); portar para BSD sockets é exercício futuro
- Sem TLS/HTTPS
- Sem cache de arquivos (lê do disco a cada requisição)
- Não suporta chunked transfer encoding

## Roadmap

- [ ] Pool de threads configurável
- [ ] Cache LRU de arquivos pequenos
- [ ] Porta para Linux (BSD sockets) com build via CMake
- [ ] Benchmark comparativo (wrk) antes/depois do pool

## Licença

[MIT](LICENSE)
