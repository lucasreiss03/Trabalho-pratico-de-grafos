#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <tuple>
#include <unordered_set>
#include <string>
#include <algorithm>
#include <climits>

struct Aresta
{
    int de, para, custo, demanda, custo_servico;
};

struct Arco
{
    int de, para, custo, demanda, custo_servico;
};

int capacidade = 0;
int deposito = 0;
int num_vertices = 0;

std::unordered_set<int> vertices_requeridos;
std::vector<Aresta> arestas_requeridas;
std::vector<Arco> arcos_requeridos;
std::vector<Aresta> arestas_opcionais;
std::vector<Arco> arcos_opcionais;

std::vector<std::vector<std::pair<int, int>>> adj_arestas;
std::vector<std::vector<std::pair<int, int>>> adj_arcos;

const int INF = 1e9;
std::vector<std::vector<int>> dist;
std::vector<std::vector<int>> pred;

void lerArquivo(const std::string &nome_arquivo)
{
    std::ifstream arq(nome_arquivo);
    std::string linha;

    enum Estado
    {
        NONE,
        REN,
        REE,
        REA,
        EDGE,
        ARC
    } estado = NONE;

    int linhasEDGE = 0;

    while (getline(arq, linha))
    {
        if (linha.empty())
            continue;

        std::stringstream ss(linha);
        std::string palavra;
        ss >> palavra;

        // Cabeçalho
        if (palavra == "Capacity:")
        {
            ss >> capacidade;
        }
        else if (palavra == "Depot")
        {
            ss >> palavra >> deposito;
        }
        else if (palavra == "#Nodes:")
        {
            ss >> num_vertices;
            adj_arestas.resize(num_vertices + 1);
            adj_arcos.resize(num_vertices + 1);
        }
        else if (palavra == "ReN.")
        {
            estado = REN;
            continue;
        }
        else if (palavra == "ReE.")
        {
            estado = REE;
            continue;
        }
        else if (palavra == "ReA.")
        {
            estado = REA;
            continue;
        }
        else if (palavra == "EDGE")
        {
            estado = EDGE;
            continue;
        }
        else if (palavra == "ARC")
        {
            estado = ARC;
            continue;
        }

        // Leitura das seções
        if (estado == REN && palavra[0] == 'N')
        {
            int id = std::stoi(palavra.substr(1));
            int demanda, custo;
            ss >> demanda >> custo;
            vertices_requeridos.insert(id);
        }
        else if (estado == REE && palavra[0] == 'E')
        {
            int de, para, custo, demanda, servico;
            ss >> de >> para >> custo >> demanda >> servico;
            arestas_requeridas.push_back({de, para, custo, demanda, servico});
            adj_arestas[de].push_back({para, custo});
            adj_arestas[para].push_back({de, custo});
        }
        else if (estado == REA && palavra[0] == 'A')
        {
            int de, para, custo, demanda, servico;
            ss >> de >> para >> custo >> demanda >> servico;
            arcos_requeridos.push_back({de, para, custo, demanda, servico});
            adj_arcos[de].push_back({para, custo});
        }
        else if (estado == EDGE)
        {
            // Divide a linha manualmente ignorando o identificador E#
            std::istringstream linha_stream(linha);
            std::string rotulo;
            int de, para, custo;

            linha_stream >> rotulo >> de >> para >> custo;

            if (!linha_stream.fail())
            {
                arestas_opcionais.push_back({de, para, custo, 0, 0});
                adj_arestas[de].push_back({para, custo});
                adj_arestas[para].push_back({de, custo});
                linhasEDGE++;
            }
        }

        else if (estado == ARC && isdigit(palavra[0]))
        {
            int de = std::stoi(palavra);
            int para, custo;
            ss >> para >> custo;
            arcos_opcionais.push_back({de, para, custo, 0, 0});
            adj_arcos[de].push_back({para, custo});
        }
    }

    arq.close();
}

int contarVertices() { return num_vertices; }
int contarArestas() { return arestas_requeridas.size() + arestas_opcionais.size(); }
int contarArcos() { return arcos_requeridos.size() + arcos_opcionais.size(); }
int contarVerticesRequeridos() { return vertices_requeridos.size(); }
int contarArestasRequeridas() { return arestas_requeridas.size(); }
int contarArcosRequeridos() { return arcos_requeridos.size(); }

double calcularDensidade()
{
    int total_arestas = contarArestas();
    int total_arcos = contarArcos();
    int V = num_vertices;

    double densidade_arestas = 0.0;
    double densidade_arcos = 0.0;

    // Densidade para parte não direcionada
    if (total_arestas > 0)
        densidade_arestas = (2.0 * total_arestas) / (V * (V - 1));

    // Densidade para parte direcionada
    if (total_arcos > 0)
        densidade_arcos = (1.0 * total_arcos) / (V * (V - 1));

    // Soma das duas densidades
    return densidade_arestas + densidade_arcos;
}

std::vector<bool> visitado;
void dfs_nao_direcionado(int u)
{
    visitado[u] = true;
    for (auto [v, _] : adj_arestas[u])
    {
        if (!visitado[v])
            dfs_nao_direcionado(v);
    }
}

int contarComponentesConexas()
{
    visitado.assign(num_vertices + 1, false);
    int comp = 0;
    for (int i = 1; i <= num_vertices; ++i)
    {
        if (!visitado[i])
        {
            dfs_nao_direcionado(i);
            comp++;
        }
    }
    return comp;
}

std::vector<std::vector<int>> grafo_reverso;
void dfs1(int u, std::vector<bool> &vis, std::vector<int> &ordem)
{
    vis[u] = true;
    for (auto [v, _] : adj_arcos[u])
        if (!vis[v])
            dfs1(v, vis, ordem);
    ordem.push_back(u);
}

void dfs2(int u, std::vector<bool> &vis)
{
    vis[u] = true;
    for (int v : grafo_reverso[u])
        if (!vis[v])
            dfs2(v, vis);
}

int contarComponentesFortementeConexas()
{
    grafo_reverso.assign(num_vertices + 1, {});
    for (int u = 1; u <= num_vertices; ++u)
        for (const auto &vizinho : adj_arcos[u])
            grafo_reverso[vizinho.first].push_back(u);

    std::vector<bool> vis(num_vertices + 1, false);
    std::vector<int> ordem;

    for (int i = 1; i <= num_vertices; ++i)
        if (!vis[i])
            dfs1(i, vis, ordem);

    std::fill(vis.begin(), vis.end(), false);
    std::reverse(ordem.begin(), ordem.end());
    int comp = 0;

    for (int u : ordem)
        if (!vis[u])
        {
            dfs2(u, vis);
            comp++;
        }

    return comp;
}

std::vector<int> calcularGrausTotais()
{
    std::vector<int> grau(num_vertices + 1, 0);

    for (int u = 1; u <= num_vertices; ++u)
        grau[u] += adj_arestas[u].size();

    for (int u = 1; u <= num_vertices; ++u)
    {
        for (const auto &vizinho : adj_arcos[u])
        {
            int v = vizinho.first;
            grau[u]++;
            grau[v]++;
        }
    }

    return grau;
}

int grauMinimo()
{
    auto graus = calcularGrausTotais();
    return *std::min_element(graus.begin() + 1, graus.end());
}

int grauMaximo()
{
    auto graus = calcularGrausTotais();
    return *std::max_element(graus.begin() + 1, graus.end());
}

void floydWarshall()
{
    int n = num_vertices;
    dist.assign(n + 1, std::vector<int>(n + 1, INF));
    pred.assign(n + 1, std::vector<int>(n + 1, -1));

    for (int i = 1; i <= n; ++i)
    {
        dist[i][i] = 0;
        pred[i][i] = i;
    }

    for (int u = 1; u <= n; ++u)
        for (auto [v, c] : adj_arestas[u])
        {
            dist[u][v] = std::min(dist[u][v], c);
            dist[v][u] = std::min(dist[v][u], c);
            pred[u][v] = u;
            pred[v][u] = v;
        }

    for (int u = 1; u <= n; ++u)
        for (auto [v, c] : adj_arcos[u])
        {
            dist[u][v] = std::min(dist[u][v], c);
            pred[u][v] = u;
        }

    for (int k = 1; k <= n; ++k)
        for (int i = 1; i <= n; ++i)
            for (int j = 1; j <= n; ++j)
                if (dist[i][k] < INF && dist[k][j] < INF && dist[i][k] + dist[k][j] < dist[i][j])
                {
                    dist[i][j] = dist[i][k] + dist[k][j];
                    pred[i][j] = pred[k][j];
                }
}

std::vector<int> calcularIntermediacao()
{
    std::vector<int> intermedia(num_vertices + 1, 0);
    for (int s = 1; s <= num_vertices; ++s)
    {
        for (int t = 1; t <= num_vertices; ++t)
        {
            if (s == t || dist[s][t] >= INF)
                continue;
            int atual = t;
            while (pred[s][atual] != s && pred[s][atual] != -1)
            {
                int anterior = pred[s][atual];
                if (anterior != s && anterior != t)
                    intermedia[anterior]++;
                atual = anterior;
            }
        }
    }
    return intermedia;
}

double calcularCaminhoMedio()
{
    long long soma = 0;
    int pares_validos = 0;
    for (int i = 1; i <= num_vertices; ++i)
    {
        for (int j = 1; j <= num_vertices; ++j)
        {
            if (i != j && dist[i][j] < INF)
            {
                soma += dist[i][j];
                pares_validos++;
            }
        }
    }
    if (pares_validos == 0)
        return -1;
    return static_cast<double>(soma) / pares_validos;
}

int calcularDiametro()
{
    int diametro = 0;
    for (int i = 1; i <= num_vertices; ++i)
    {
        for (int j = 1; j <= num_vertices; ++j)
        {
            if (i != j && dist[i][j] < INF)
            {
                diametro = std::max(diametro, dist[i][j]);
            }
        }
    }
    return diametro;
}

void menu()
{
    std::cout << "\nEscolha uma opcao para calcular (1 a 13) ou 0 para sair:\n";
    std::cout << "1. Quantidade de vertices\n";
    std::cout << "2. Quantidade de arestas\n";
    std::cout << "3. Quantidade de arcos\n";
    std::cout << "4. Quantidade de vertices requeridos\n";
    std::cout << "5. Quantidade de arestas requeridas\n";
    std::cout << "6. Quantidade de arcos requeridos\n";
    std::cout << "7. Densidade do grafo\n";
    std::cout << "8. Componentes conectados\n";
    std::cout << "9. Grau minimo dos vertices\n";
    std::cout << "10. Grau maximo dos vertices\n";
    std::cout << "11. Intermediacao de cada vertice\n";
    std::cout << "12. Caminho medio\n";
    std::cout << "13. Diametro\n";
    std::cout << "0. Sair\n> ";
}

int main()
{
    lerArquivo("DI-NEARP-n422-Q8k.dat");
    floydWarshall();

    int opcao;
    do
    {
        menu();
        std::cin >> opcao;

        switch (opcao)
        {
        case 1:
            std::cout << "Vertices: " << contarVertices() << "\n";
            break;
        case 2:
            std::cout << "Arestas: " << contarArestas() << "\n";
            break;
        case 3:
            std::cout << "Arcos: " << contarArcos() << "\n";
            break;
        case 4:
            std::cout << "Vertices requeridos: " << contarVerticesRequeridos() << "\n";
            break;
        case 5:
            std::cout << "Arestas requeridas: " << contarArestasRequeridas() << "\n";
            break;
        case 6:
            std::cout << "Arcos requeridos: " << contarArcosRequeridos() << "\n";
            break;
        case 7:
            std::cout << "Densidade: " << calcularDensidade() << "\n";
            break;
        case 8:
            std::cout << "Componentes conexas: " << contarComponentesConexas() << "\n";
            break;
        case 9:
            std::cout << "Grau minimo: " << grauMinimo() << "\n";
            break;
        case 10:
            std::cout << "Grau maximo: " << grauMaximo() << "\n";
            break;
        case 11:
        {
            auto intermedia = calcularIntermediacao();
            for (int i = 1; i <= num_vertices; ++i)
                std::cout << "Intermediacao vertice " << i << ": " << intermedia[i] << "\n";
            break;
        }
        case 12:
            std::cout << "Caminho medio: " << calcularCaminhoMedio() << "\n";
            break;
        case 13:
            std::cout << "Diametro: " << calcularDiametro() << "\n";
            break;
        case 0:
            std::cout << "Encerrando...\n";
            break;
        default:
            std::cout << "Opcao invalida.\n";
            break;
        }
    } while (opcao != 0);

    return 0;
}
