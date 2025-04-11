
# 📊 Analisador de Grafos DI-NEARP

Este projeto implementa um analisador de grafos mistos com leitura a partir de arquivos no formato **DI-NEARP**, realizando cálculos de estatísticas estruturais relevantes. A solução foi desenvolvida em **C++**, com suporte adicional em **Python** para validação visual dos resultados.

---

## 📦 Estrutura do Projeto

```
.
├── trabalho.cpp                     # Código principal em C++
├── DI-NEARP-n422-Q8k.dat           # Arquivo de entrada com o grafo
├── saida.txt                       # Arquivo de saída com estatísticas
├── visualizacao_de_arquivo.ipynb  # Notebook Python para exibir estatísticas
```

---

## 🧠 Funcionalidades Implementadas

O projeto em C++ é capaz de:

- Ler arquivos `.dat` no formato DI-NEARP
- Construir estruturas para:
  - Vértices requeridos
  - Arestas (requeridas e opcionais)
  - Arcos (requeridos e opcionais)
- Calcular automaticamente as seguintes estatísticas:

| Estatística                                  | Descrição                                                                 |
|---------------------------------------------|---------------------------------------------------------------------------|
| 1. Quantidade de vértices                   | Total de nós declarados no arquivo                                       |
| 2. Quantidade de arestas                    | Soma das arestas requeridas e opcionais                                 |
| 3. Quantidade de arcos                      | Soma dos arcos requeridos e opcionais                                   |
| 4. Vértices requeridos                      | Conjunto de vértices com demanda                                         |
| 5. Arestas requeridas                       | Arestas com custo de serviço e demanda                                  |
| 6. Arcos requeridos                         | Arcos com custo de serviço e demanda                                    |
| 7. Densidade                                | Calculada separadamente para a parte direcionada e não-direcionada      |
| 8. Componentes conexas                      | Calculadas por DFS no subgrafo não-direcionado                          |
| 9. Grau mínimo e máximo                     | Considerando conexões em arestas e arcos                                |
| 10. Intermediação                           | Número de vezes que um vértice está no caminho mínimo entre outros dois |
| 11. Caminho médio                           | Média das distâncias entre pares de vértices alcançáveis                |
| 12. Diâmetro                                | Maior distância entre quaisquer dois vértices alcançáveis               |

---

## 🧮 Como Executar

### 🔧 Compilação (C++)

> Versão usada no desenvolvimento: `g++ (GCC) 13.2.0`
> 
> ```
> g++ (GCC) 13.2.0
> Copyright (C) 2023 Free Software Foundation, Inc.
> This is free software; see the source for copying conditions.
> There is NO warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
> ```

```bash
g++ trabalho.cpp -o grafo
```

### ▶️ Execução

```bash
./grafo
```

> O programa automaticamente:
> - Lê o arquivo `DI-NEARP-n422-Q8k.dat`
> - Executa o algoritmo de Floyd-Warshall
> - Gera o arquivo `saida.txt` com todas as estatísticas
> - Permite ao usuário consultar as estatísticas individualmente via menu

---

## 🧪 Validação com Python

O notebook `visualizacao_de_arquivo.ipynb` **não reconstrói o grafo nem realiza cálculos diretamente sobre ele**. Seu objetivo principal é **ler e exibir de forma organizada as estatísticas geradas pelo programa em C++**.

### ✅ O que ele faz:

- Abre o arquivo `saida.txt` gerado automaticamente após a execução do programa em C++
- Usa expressões regulares para extrair os pares "Estatística: Valor"
- Exibe os dados em uma tabela com `pandas.DataFrame` para facilitar a leitura

### 📘 Bibliotecas utilizadas:

```python
import pandas as pd
import re
import os
```

### ⚠️ Requisitos:

- O arquivo `saida.txt` precisa existir no mesmo diretório que o notebook
- O programa em C++ deve ser executado antes, para gerar o arquivo

### 💡 Exemplo de saída:

| Estatística                | Valor     |
|---------------------------|-----------|
| 1. Quantidade de vertices | 25        |
| 2. Quantidade de arestas  | 38        |
| ...                       | ...       |

> Essa etapa é útil para conferência visual dos dados processados, sem a necessidade de abrir o arquivo `.txt` manualmente.

---

## 🗂️ Formato de Entrada Esperado

O programa espera arquivos com seções específicas:

```text
#Nodes: 10
Depot 1
Capacity: 30
ReN.
N1 5 0
N3 3 0
ReE.
E1 1 2 10 2 1
ReA.
A1 2 3 7 3 1
EDGE
E5 1 3 4
ARC
2 1 5
```

Cada seção identifica:

- `ReN.`: vértices requeridos
- `ReE.`: arestas requeridas
- `ReA.`: arcos requeridos
- `EDGE`: arestas opcionais
- `ARC`: arcos opcionais

---

## 🧾 Saída

- O resultado completo é salvo no arquivo `saida.txt`
- Também pode ser exibido no terminal usando o menu interativo do programa

