# Registro de uso de IA

Preencha durante o trabalho, não no fim. É o portão G4 da avaliação
([desafio](../docs/desafio.md#portões)): os dois alunos da dupla precisam ter
entradas. Quem não usou IA escreve isso aqui.

## Dupla

| Aluno | Matrícula | Partes que escreveu |
|---|---|---|
| Erick de Lima Mascarenhas | 23211622 | Interfaces dos módulos (`hw.h`, `line.h`, `payment.h`, `tariff.h`, `display.h`), montador de quadros UART (`line.c`) e sua suíte `tests/line`, camada de hardware (`hw.c`), ADC4 (`distance.c`), câmera (`camera.c`), sessões (`session.c`), máquina de estados das faixas (`lane.c`), `main.c` e as suítes `tests/camera`, `tests/session`, `tests/distance`, `tests/lane` e `tests/hw` |
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

### 2026-11-20 — Camada de hardware, ADC4, câmera, sessões e máquina de estados

- **Aluno:** Erick de Lima Mascarenhas
- **Requisito(s):** REQ-01, REQ-02, REQ-03, REQ-06, REQ-07, REQ-08, REQ-09, REQ-10, REQ-13
- **O que pedi:** implementar a parte do controlador que não depende do pagamento: drivers, ADC, câmera, sessões e as duas faixas, priorizando os cenários de segurança.
- **O que a IA propôs:** `hw.c` como único ponto com drivers (GPIO/EXTI com a borda adiada para a work queue, ADC4 assíncrono, UART por polling, SPI do display); `distance.c` como dono único do ADC4, alternando os canais 17 e 18, com prazo de 100 ms e falha travada; `camera.c` com o pedido `AT+CAPTURE` e a validação do `+PLATE`; `session.c` com a tabela por placa; `lane.c` com uma máquina de estados para as duas faixas (BOOT, IDLE, IDENTIFYING, PAYING, OPENING, OPEN, PASSING, CLOSING, ASSIST, FAULT); `main.c` com laço de 1 ms e mutex compartilhado com a work queue.
- **O que aceitei, mudei ou rejeitei, e por quê:** aceitei. A IA trocou a recepção UART por interrupção pela leitura por polling, porque a documentação do laboratório (`hardware-models.md`) não garante transporte UART por IRQ no modelo. Separei a assistência que rearma (cancela fechada) da falha travada (cancela fora do fim de curso, limites contraditórios, prazos do motor, ADC), como pede o REQ-08.
- **Como verifiquei:** 13 cenários públicos que não dependem do pagamento passando (`camera-timeout`, `confidence-retry`, `contradictory-limits`, `jam`, `low-confidence`, `missing-lower`, `missing-upper`, `noisy-threshold`, `obstruction-before-close`, `obstruction-during-close`, `partial-startup`, `reset-capture`, `stationary`); `normal` correto até a entrada, falhando só na saída por falta do pagamento; `scripts/static-check.py` com 0 achados.
- **O que aprendi / o que ainda preciso entender:** por que a borda do fim de curso é tratada na work queue e não na ISR; por que a falha do ADC trava as duas faixas; por que a faixa espera 20 ms no boot antes de ler os sensores.

### 2026-11-20 — Suítes de unidade e cobertura

- **Aluno:** Erick de Lima Mascarenhas
- **Requisito(s):** REQ-02, REQ-03, REQ-06, REQ-07, REQ-08, REQ-09, REQ-10, REQ-11
- **O que pedi:** testes de unidade com FFF para os meus módulos, cobrindo os caminhos de erro que os cenários não alcançam.
- **O que a IA propôs:** uma suíte por módulo. `lane` usa falsos do FFF de `hw.h`, `distance.h`, `payment.h`, `tariff.h` e `display.h` lendo um "mundo" simulado, com câmera, sessões e montador reais; `distance` usa falsos do ADC; `hw` roda com os drivers e modelos reais. Para reduzir branches que só um driver com defeito alcançaria, `hw_init()` passou a acumular o primeiro erro (`first_error`) em vez de testar cada passo; `distance_init()` e `lane_init()` zeram o estado, o que as suítes usam entre testes. A suíte de exemplo `tests/unit` foi removida.
- **O que aceitei, mudei ou rejeitei, e por quê:** aceitei. As seis linhas excluídas (`LCOV_EXCL`, 1,4%) são falhas de driver e de inicialização que o modelo não produz, cada uma com o motivo no código.
- **Como verifiquei:** `scripts/firmware-coverage.py` com as seis suítes e os 13 cenários que não dependem do pagamento passando: meus arquivos com 100% das linhas; branches com 100%, exceto `hw.c` (84,62%, quatro desvios de erro de driver). `scripts/static-check.py` com 0 achados.
- **O que aprendi / o que ainda preciso entender:** como o FFF substitui uma função na ligação; por que um teste que roda vários casos num laço precisa restaurar o "mundo" a cada iteração.

## Código de terceiros

Trechos copiados ou adaptados de outras fontes (documentação, exemplos do
Zephyr, fóruns), com origem e licença.

| Arquivo/trecho | Origem | Licença |
|---|---|---|
| | | |
