# Registro de uso de IA

Preencha durante o trabalho, não no fim. É o portão G4 da avaliação
([desafio](../docs/desafio.md#portões)): os dois alunos da dupla precisam ter
entradas. Quem não usou IA escreve isso aqui.

## Dupla

| Aluno | Matrícula | Partes que escreveu |
|---|---|---|
| Erick de Lima Mascarenhas | 23211622 | Interfaces dos módulos (`hw.h`, `line.h`, `payment.h`, `tariff.h`, `display.h`), montador de quadros UART (`line.c`) e sua suíte `tests/line` |
| João Victor Cavalcante da Silva Correia | 2021301012 | (a preencher pelo João) |

## Ferramentas

| Aluno | Ferramenta | Modelo/versão | Período de uso |
|---|---|---|---|
| Erick de Lima Mascarenhas | Claude Code | Claude Opus 5.5 (`claude-opus-5-5`) | 2026-11-18 em diante |

## Tarefas

Uma entrada por tarefa relevante (um requisito, um bug, uma refatoração).

### 2026-11-18 — Ambiente do laboratório no WSL

- **Aluno:** Erick de Lima Mascarenhas
- **Requisito(s):** nenhum (ambiente)
- **O que pedi:** montar o ambiente do guia de instalação no WSL 2 (Ubuntu 24.04, x86_64) e rodar o primeiro teste.
- **O que a IA propôs:** seguir o `docs/instalacao.md` e conferir o SHA-256 do Renode com o `docs/setup.md`. O `lab.py` recusava o Renode oficial x86_64 ("Wrong runtime identity", data de build 20260907 contra 20260906). A IA tentou um script intermediário que reescrevia a data; não autorizei contornar a verificação.
- **O que aceitei, mudei ou rejeitei, e por quê:** rejeitei qualquer contorno. Reportei o problema ao professor, que o corrigiu na v1.0.1; mesclei a v1.0.1 no branch `solucao`.
- **Como verifiquei:** cenário `normal` com o código inicial falhou com a mensagem esperada pelo guia (`checkpoint at 5.90 s (tick 590): position_mdeg expected 90000, observed 0`).
- **O que aprendi / o que ainda preciso entender:** o laboratório confere a identidade do Renode antes de compilar; os prazos são em tempo virtual.

### 2026-11-20 — Interfaces e montador de quadros UART

- **Aluno:** Erick de Lima Mascarenhas
- **Requisito(s):** REQ-11
- **O que pedi:** dividir o projeto em módulos com interfaces fixas, para a dupla trabalhar em paralelo, e implementar a montagem de quadros que os dois analisadores UART usam.
- **O que a IA propôs:** camada fina de hardware (`hw.h`) como único ponto que chama drivers; interfaces de pagamento, tarifa e display para o João; e `line.c`: quadro de até 128 bytes com terminador, só ASCII imprimível, CR LF na câmera e só LF no pagamento, prazo de 1000 ms desde o primeiro byte e descarte até o LF, sem entregar prefixo.
- **O que aceitei, mudei ou rejeitei, e por quê:** aceitei a divisão por módulo (não por faixa, como o desafio recomenda) e o montador como está.
- **Como verifiquei:** suíte `tests/line` passando no Renode via `scripts/firmware-coverage.py`; `line.c` com 100% das linhas e 97,37% dos branches.
- **O que aprendi / o que ainda preciso entender:** por que o CR só pode vir imediatamente antes do LF e por que o espaço do LF é reservado ao guardar cada byte.

## Código de terceiros

Trechos copiados ou adaptados de outras fontes (documentação, exemplos do
Zephyr, fóruns), com origem e licença.

| Arquivo/trecho | Origem | Licença |
|---|---|---|
| | | |
