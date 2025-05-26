/*
    Helder Avila
    Lucas Reis Silvino
*/

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <map>
#include <queue>
#include <set>
#include <string>
#include <filesystem>
#include <chrono>
#include <tuple>
#include <limits>
#include <algorithm>

const int INF = INT_MAX;

struct Servico
{
    int id;
    int origem, destino;
    int custo, demanda;
    bool atendido = false;
};

struct NoRequerido
{
    int id, demanda, custo;
    int servico_id;
    bool atendido = false;
};

typedef std::pair<int, int> Par;
typedef std::vector<std::vector<Par>> ListaAdj;

class Instancia
{
private:
    int capacidadeVeiculo = 0;
    int deposito = 0;
    int qtdVertices = 0;
    std::string nomeBase;

    std::vector<NoRequerido> nosRequeridos;
    std::vector<Servico> arestasRequeridas;
    std::vector<Servico> arcosRequeridos;
    std::map<int, int> mapaDemanda;
    std::map<int, int> mapaCusto;
    ListaAdj grafo;
    std::vector<std::vector<int>> distancias;

    using Rota = std::vector<std::tuple<int, int, int, int, int>>;
    std::vector<Rota> melhorSolucao;

    void resetarServicos()
    {
        for (auto &no : nosRequeridos)
            no.atendido = false;
        for (auto &e : arestasRequeridas)
            e.atendido = false;
        for (auto &a : arcosRequeridos)
            a.atendido = false;
    }

    void construirMapas()
    {
        for (const auto &no : nosRequeridos)
        {
            mapaDemanda[no.servico_id] = no.demanda;
            mapaCusto[no.servico_id] = no.custo;
        }
        for (const auto &e : arestasRequeridas)
        {
            mapaDemanda[e.id] = e.demanda;
            mapaCusto[e.id] = e.custo;
        }
        for (const auto &a : arcosRequeridos)
        {
            mapaDemanda[a.id] = a.demanda;
            mapaCusto[a.id] = a.custo;
        }
    }

    void dijkstra(int origem, std::vector<int> &dist)
    {
        dist.assign(qtdVertices + 1, INF);
        dist[origem] = 0;
        std::priority_queue<Par, std::vector<Par>, std::greater<Par>> fila;
        fila.push({0, origem});

        while (!fila.empty())
        {
            auto [custo, u] = fila.top();
            fila.pop();
            if (custo > dist[u])
                continue;
            for (auto &[v, w] : grafo[u])
            {
                if (dist[u] + w < dist[v])
                {
                    dist[v] = dist[u] + w;
                    fila.push({dist[v], v});
                }
            }
        }
    }

    int custoTotal(const std::vector<Rota> &rotas)
    {
        int custo = 0;
        for (const auto &rota : rotas)
            for (auto &[tipo, id, u, v, c] : rota)
                if (tipo == 1)
                    custo += mapaCusto[id];
        return custo;
    }

public:
    void lerArquivo(const std::string &caminho)
    {
        std::ifstream arquivo(caminho);
        std::string linha;
        int id_servico = 1;

        nomeBase = std::filesystem::path(caminho).stem().string();
        nosRequeridos.clear();
        arestasRequeridas.clear();
        arcosRequeridos.clear();
        mapaDemanda.clear();
        mapaCusto.clear();

        while (std::getline(arquivo, linha))
        {
            if (linha.rfind("Capacity:", 0) == 0)
                capacidadeVeiculo = std::stoi(linha.substr(9));
            else if (linha.rfind("Depot Node:", 0) == 0)
                deposito = std::stoi(linha.substr(12));
            else if (linha.rfind("#Nodes:", 0) == 0)
            {
                qtdVertices = std::stoi(linha.substr(8));
                grafo.assign(qtdVertices + 1, {});
            }
            else if (linha.rfind("ReN.", 0) == 0)
            {
                while (std::getline(arquivo, linha) && !linha.empty() && linha[0] == 'N')
                {
                    std::stringstream ss(linha);
                    std::string id;
                    int demanda, custo;
                    ss >> id >> demanda >> custo;
                    int no = std::stoi(id.substr(1));
                    nosRequeridos.push_back({no, demanda, custo, id_servico++, false});
                }
            }
            else if (linha.rfind("ReE.", 0) == 0)
            {
                while (std::getline(arquivo, linha) && !linha.empty() && linha[0] == 'E')
                {
                    std::stringstream ss(linha);
                    std::string id;
                    int u, v, custo, demanda, scusto;
                    ss >> id >> u >> v >> custo >> demanda >> scusto;
                    arestasRequeridas.push_back({id_servico++, u, v, custo, demanda});
                    grafo[u].emplace_back(v, custo);
                    grafo[v].emplace_back(u, custo);
                }
            }
            else if (linha.rfind("ReA.", 0) == 0)
            {
                while (std::getline(arquivo, linha) && !linha.empty() && linha[0] == 'A')
                {
                    std::stringstream ss(linha);
                    std::string id;
                    int u, v, custo, demanda, scusto;
                    ss >> id >> u >> v >> custo >> demanda >> scusto;
                    arcosRequeridos.push_back({id_servico++, u, v, custo, demanda});
                    grafo[u].emplace_back(v, custo);
                }
            }
            else if (linha.rfind("EDGE.", 0) == 0)
            {
                while (std::getline(arquivo, linha) && !linha.empty() && linha[0] == 'E')
                {
                    std::stringstream ss(linha);
                    std::string id;
                    int u, v, custo, dummy1, dummy2;
                    ss >> id >> u >> v >> custo >> dummy1 >> dummy2;
                    grafo[u].emplace_back(v, custo);
                    grafo[v].emplace_back(u, custo);
                }
            }
            else if (linha.rfind("ARC.", 0) == 0)
            {
                while (std::getline(arquivo, linha) && !linha.empty() && linha[0] == 'A')
                {
                    std::stringstream ss(linha);
                    std::string id;
                    int u, v, custo, dummy1, dummy2;
                    ss >> id >> u >> v >> custo >> dummy1 >> dummy2;
                    grafo[u].emplace_back(v, custo);
                }
            }
        }

        construirMapas();
        distancias.assign(qtdVertices + 1, std::vector<int>(qtdVertices + 1, INF));
        for (int i = 1; i <= qtdVertices; i++)
            dijkstra(i, distancias[i]);
    }

    void construirRotas()
    {
        std::map<int, std::tuple<int, int, int, int, int>> todosServicos;
        for (auto &n : nosRequeridos)
            todosServicos[n.servico_id] = std::make_tuple(1, n.servico_id, n.id, n.id, n.custo);
        for (auto &e : arestasRequeridas)
            todosServicos[e.id] = std::make_tuple(1, e.id, e.origem, e.destino, e.custo);
        for (auto &a : arcosRequeridos)
            todosServicos[a.id] = std::make_tuple(1, a.id, a.origem, a.destino, a.custo);

        melhorSolucao.clear();
        int menorCusto = INF;

        for (int regra = 1; regra <= 3; regra++)
        {
            resetarServicos();
            std::set<int> pendentes;
            for (auto &[id, _] : todosServicos)
                pendentes.insert(id);
            std::vector<Rota> rotas;

            while (!pendentes.empty())
            {
                int capacidadeRestante = capacidadeVeiculo;
                Rota rota;
                int atual = deposito;
                rota.emplace_back(0, 0, 0, 0, 0);

                while (true)
                {
                    int melhor = -1;
                    double prioridade = INF;

                    for (int id : pendentes)
                    {
                        int u = atual;
                        int v = std::get<2>(todosServicos[id]);
                        int d = mapaDemanda[id];
                        int c = mapaCusto[id];
                        if (d > capacidadeRestante)
                            continue;

                        double crit;
                        if (regra == 1)
                            crit = distancias[u][v];
                        else if (regra == 2)
                            crit = -d;
                        else
                            crit = (double)c / d;

                        if (crit < prioridade)
                        {
                            prioridade = crit;
                            melhor = id;
                        }
                    }

                    if (melhor == -1)
                        break;

                    capacidadeRestante -= mapaDemanda[melhor];
                    rota.push_back(todosServicos[melhor]);
                    pendentes.erase(melhor);
                    atual = std::get<3>(todosServicos[melhor]);
                }

                rota.emplace_back(0, 0, 0, 0, 0);
                if (rota.size() > 2)
                    rotas.push_back(rota);
                else
                    break;
            }

            int custoAtual = custoTotal(rotas);
            if (custoAtual < menorCusto)
            {
                menorCusto = custoAtual;
                melhorSolucao = rotas;
            }
        }
    }

    void salvarSolucao(long long clocks)
    {
        std::string nomeArquivo = "sol-" + nomeBase + ".dat";
        std::ofstream out(nomeArquivo);

        int custoTotal = 0;
        for (const auto &rota : melhorSolucao)
            for (auto &[tipo, id, u, v, custo] : rota)
                if (tipo == 1)
                    custoTotal += mapaCusto[id];

        out << custoTotal << "\n";
        out << melhorSolucao.size() << "\n";
        out << clocks << "\n"
            << clocks << "\n";

        int rota_id = 1;
        for (const auto &rota : melhorSolucao)
        {
            int demanda = 0, custo = 0;
            for (auto &[tipo, id, u, v, c] : rota)
            {
                if (tipo == 1)
                {
                    demanda += mapaDemanda[id];
                    custo += mapaCusto[id];
                }
            }
            out << "0 1 " << rota_id++ << " " << demanda << " " << custo << " " << rota.size();
            for (auto &[tipo, id, u, v, _] : rota)
            {
                if (tipo == 0)
                    out << " (D " << u << ",1,1)";
                else
                    out << " (S " << id << "," << u << "," << v << ")";
            }
            out << "\n";
        }
        out.close();
    }
};

int main()
{
    std::string pasta = "dados/";
    for (const auto &entrada : std::filesystem::directory_iterator(pasta))
    {
        if (entrada.path().extension() == ".dat")
        {
            std::cout << "Processando: " << entrada.path().filename() << std::endl;
            Instancia instancia;
            instancia.lerArquivo(entrada.path().string());
            auto ini = std::chrono::high_resolution_clock::now();
            instancia.construirRotas();
            auto fim = std::chrono::high_resolution_clock::now();
            long long tempo = std::chrono::duration_cast<std::chrono::nanoseconds>(fim - ini).count();
            instancia.salvarSolucao(tempo);
        }
    }
    std::cout << "Solucoes geradas com Sucesso!!!\n";
    return 0;
}
