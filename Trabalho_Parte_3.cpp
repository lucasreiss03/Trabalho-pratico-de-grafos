#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <map>
#include <queue>
#include <set>
#include <string>
#include <chrono>
#include <tuple>
#include <limits>
#include <algorithm>
#include <random>
#include <future>
#include <numeric>
#include <unordered_map>
#include <unordered_set>
#include <thread>
#include <atomic>
#include <mutex>
#include <filesystem>
#include <windows.h>

const short INF = 32767;

struct Servico
{
    short id, origem, destino, custo, demanda;
};

struct NoRequerido
{
    short id, demanda, custo, servico_id;
};

struct Aresta
{
    short u, v, peso;
};

typedef std::pair<short, short> Par;
typedef std::vector<std::vector<Par>> ListaAdj;
using Rota = std::vector<std::tuple<short, short, short, short, short>>;

class Instancia
{
private:
    short capacidadeVeiculo = 0, deposito = 0, qtdVertices = 0;
    std::string nomeBase;
    std::vector<NoRequerido> nosRequeridos;
    std::vector<Servico> arestasRequeridas, arcosRequeridos;

    std::vector<short> mapaDemanda, mapaCusto;
    short maxServicoId = 0;

    ListaAdj grafo;

    // Cache de distâncias thread-safe
    std::vector<std::vector<short>> distanciasCache;
    std::vector<std::atomic<bool> *> distanciasCalculadas;
    std::vector<std::unique_ptr<std::mutex>> distanciasMutex;

    std::vector<Rota> melhorSolucao;
    std::atomic<int> melhorCustoGlobal{32767};
    std::mutex melhorSolucaoMutex;

public:
    // Destrutor para liberar memória dos atomic<bool>*
    ~Instancia()
    {
        for (auto *ptr : distanciasCalculadas)
        {
            delete ptr;
        }
    }

    void construirMapas()
    {
        for (const auto &no : nosRequeridos)
            maxServicoId = std::max(maxServicoId, no.servico_id);
        for (const auto &e : arestasRequeridas)
            maxServicoId = std::max(maxServicoId, e.id);
        for (const auto &a : arcosRequeridos)
            maxServicoId = std::max(maxServicoId, a.id);

        mapaDemanda.assign(maxServicoId + 1, 0);
        mapaCusto.assign(maxServicoId + 1, 0);

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

    // Dijkstra thread-safe com double-checked locking
    const std::vector<short> &obterDistancias(short origem)
    {
        if (!distanciasCalculadas[origem]->load(std::memory_order_acquire))
        {
            std::lock_guard<std::mutex> lock(*distanciasMutex[origem]);
            if (!distanciasCalculadas[origem]->load(std::memory_order_relaxed))
            {
                dijkstra(origem, distanciasCache[origem]);
                distanciasCalculadas[origem]->store(true, std::memory_order_release);
            }
        }
        return distanciasCache[origem];
    }

    void dijkstra(short origem, std::vector<short> &dist)
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

    int custoTotal(const std::vector<Rota> &rotas) const
    {
        int custo = 0;
        for (const auto &rota : rotas)
            for (const auto &[tipo, id, u, v, c] : rota)
                if (tipo == 1)
                    custo += mapaCusto[id];
        return custo;
    }

    short calcularCargaRota(const Rota &rota) const
    {
        short carga = 0;
        for (const auto &[tipo, id, u, v, c] : rota)
        {
            if (tipo == 1)
                carga += mapaDemanda[id];
        }
        return carga;
    }

    // 2-opt otimizado com parada antecipada - MELHORIA 2
    bool aplicar2Opt(Rota &rota)
    {
        if (rota.size() <= 4)
            return false;

        bool melhorou = false;
        for (size_t i = 1; i < rota.size() - 2; ++i)
        {
            bool melhorouNivelI = false;
            for (size_t j = i + 1; j < rota.size() - 1; ++j)
            {
                // Calcular custo atual das conexões que serão quebradas/criadas
                auto &elem_i = rota[i];
                auto &elem_i_next = rota[i + 1];
                auto &elem_j = rota[j];
                auto &elem_j_next = rota[j + 1];

                // Estimativa simples de melhoria baseada em distâncias
                short u1 = std::get<3>(elem_i), v1 = std::get<2>(elem_i_next);
                short u2 = std::get<3>(elem_j), v2 = std::get<2>(elem_j_next);

                const auto &dist_u1 = obterDistancias(u1);
                const auto &dist_u2 = obterDistancias(u2);

                short custoAtual = dist_u1[v1] + dist_u2[v2];
                short novoCusto = dist_u1[u2] + dist_u2[v1];

                if (novoCusto < custoAtual - 5) // threshold mínimo
                {
                    std::reverse(rota.begin() + i + 1, rota.begin() + j + 1);
                    melhorou = melhorouNivelI = true;
                    break; // Para assim que encontrar melhoria significativa
                }
            }
            if (melhorouNivelI)
                break; // Move para próximo i se melhorou
        }
        return melhorou;
    }

    // Or-opt otimizado - move apenas segmentos que realmente melhoram
    bool aplicarOrOpt(Rota &rota)
    {
        if (rota.size() <= 4)
            return false;

        // Testar segmentos de tamanho 1 e 2 (mais eficiente)
        for (int segSize = 1; segSize <= std::min(2, (int)rota.size() - 3); ++segSize)
        {
            for (size_t i = 1; i < rota.size() - segSize; ++i)
            {
                // Calcular custo de remoção
                short custoRemocao = 0;
                if (i > 1 && i + segSize < rota.size() - 1)
                {
                    short u_antes = std::get<3>(rota[i - 1]);
                    short v_depois = std::get<2>(rota[i + segSize]);
                    custoRemocao = obterDistancias(u_antes)[v_depois];
                }

                // Extrair segmento
                std::vector<std::tuple<short, short, short, short, short>> segmento(
                    rota.begin() + i, rota.begin() + i + segSize);
                rota.erase(rota.begin() + i, rota.begin() + i + segSize);

                bool melhorou = false;
                size_t melhorPos = i;
                short melhorGanho = 0;

                // Testar posições próximas primeiro (mais provável de melhorar)
                std::vector<size_t> posicoes;
                for (size_t pos = 1; pos < rota.size(); ++pos)
                    posicoes.push_back(pos);

                // Ordenar por distância da posição original
                std::sort(posicoes.begin(), posicoes.end(),
                          [i](size_t a, size_t b)
                          { return abs((int)a - (int)i) < abs((int)b - (int)i); });

                for (size_t pos : posicoes)
                {
                    if (pos >= i && pos <= i + segSize)
                        continue;

                    // Calcular custo de inserção
                    short custoInsercao = 0;
                    if (pos > 0 && pos < rota.size())
                    {
                        short u_antes = (pos > 0) ? std::get<3>(rota[pos - 1]) : deposito;
                        short v_seg = std::get<2>(segmento[0]);
                        short u_seg = std::get<3>(segmento.back());
                        short v_depois = (pos < rota.size()) ? std::get<2>(rota[pos]) : deposito;

                        const auto &dist_antes = obterDistancias(u_antes);
                        const auto &dist_seg = obterDistancias(u_seg);
                        custoInsercao = dist_antes[v_seg] + dist_seg[v_depois];
                    }

                    short ganho = custoRemocao - custoInsercao;
                    if (ganho > melhorGanho)
                    {
                        melhorGanho = ganho;
                        melhorPos = pos;
                        melhorou = true;
                    }
                }

                if (melhorou)
                {
                    rota.insert(rota.begin() + melhorPos, segmento.begin(), segmento.end());
                    return true;
                }
                else
                {
                    // Restaurar posição original
                    rota.insert(rota.begin() + i, segmento.begin(), segmento.end());
                }
            }
        }
        return false;
    }

    // Or-opt inter-rotas otimizado
    bool aplicarOrOptInterRotas(std::vector<Rota> &rotas)
    {
        for (size_t r1 = 0; r1 < rotas.size(); ++r1)
        {
            if (rotas[r1].size() <= 2)
                continue;

            for (size_t i = 1; i < rotas[r1].size() - 1; ++i)
            {
                if (std::get<0>(rotas[r1][i]) != 1)
                    continue;

                short demandaServico = mapaDemanda[std::get<1>(rotas[r1][i])];

                // Procurar rota com menor carga que possa receber o serviço
                size_t melhorRota = rotas.size();
                short menorCarga = capacidadeVeiculo + 1;

                for (size_t r2 = 0; r2 < rotas.size(); ++r2)
                {
                    if (r1 == r2)
                        continue;

                    short cargaR2 = calcularCargaRota(rotas[r2]);
                    if (cargaR2 + demandaServico <= capacidadeVeiculo && cargaR2 < menorCarga)
                    {
                        menorCarga = cargaR2;
                        melhorRota = r2;
                    }
                }

                if (melhorRota < rotas.size())
                {
                    auto servico = rotas[r1][i];
                    rotas[r1].erase(rotas[r1].begin() + i);

                    // Inserir na melhor posição da rota escolhida
                    size_t melhorPos = 1;
                    rotas[melhorRota].insert(rotas[melhorRota].begin() + melhorPos, servico);

                    return true;
                }
            }
        }
        return false;
    }

    // VNS simplificado com critério de parada baseado em melhoria - MELHORIA 5
    bool vnsParalelo(std::vector<Rota> &rotas)
    {
        bool melhorou = false;

        // 1. Aplicar 2-opt em todas as rotas
        for (auto &rota : rotas)
        {
            if (aplicar2Opt(rota))
                melhorou = true;
        }

        // 2. Aplicar Or-opt intra-rota
        for (auto &rota : rotas)
        {
            if (aplicarOrOpt(rota))
                melhorou = true;
        }

        // 3. Or-opt inter-rotas
        if (aplicarOrOptInterRotas(rotas))
            melhorou = true;

        return melhorou;
    }

    // Construção gulosa com inserção na melhor posição - MELHORIA 6
    std::vector<Rota> construcaoGulosaSimplesEficiente(const std::vector<Servico> &servicos)
    {
        std::vector<bool> servicosUsados(maxServicoId + 1, false);
        std::vector<Rota> rotas;

        std::vector<std::pair<double, int>> eficiencias;
        eficiencias.reserve(servicos.size());

        // Calcular eficiência considerando distância ao depósito - MELHORIA 1
        const auto &distDeposito = obterDistancias(deposito);
        for (size_t i = 0; i < servicos.size(); ++i)
        {
            const auto &s = servicos[i];
            double ef = (double)s.custo / std::max((short)1, s.demanda);
            double fatorDistancia = 1.0 + (distDeposito[s.origem] + distDeposito[s.destino]) / 200.0;
            ef *= fatorDistancia;
            eficiencias.emplace_back(ef, i);
        }

        std::sort(eficiencias.begin(), eficiencias.end());

        for (auto [ef, idx] : eficiencias)
        {
            const auto &s = servicos[idx];
            if (servicosUsados[s.id])
                continue;

            // Encontrar melhor posição de inserção
            bool adicionado = false;
            short melhorCustoInsercao = INF;
            size_t melhorRota = rotas.size();
            size_t melhorPos = 0;

            for (size_t r = 0; r < rotas.size(); ++r)
            {
                short cargaAtual = calcularCargaRota(rotas[r]);
                if (cargaAtual + s.demanda > capacidadeVeiculo)
                    continue;

                // Testar inserção em cada posição
                for (size_t pos = 1; pos < rotas[r].size(); ++pos)
                {
                    short u_ant = (pos > 1) ? std::get<3>(rotas[r][pos - 1]) : deposito;
                    short v_prox = (pos < rotas[r].size() - 1) ? std::get<2>(rotas[r][pos]) : deposito;

                    const auto &dist_ant = obterDistancias(u_ant);
                    const auto &dist_serv = obterDistancias(s.destino);

                    short custoInsercao = dist_ant[s.origem] + dist_serv[v_prox];
                    if (pos < rotas[r].size() - 1)
                        custoInsercao -= obterDistancias(u_ant)[v_prox];

                    if (custoInsercao < melhorCustoInsercao)
                    {
                        melhorCustoInsercao = custoInsercao;
                        melhorRota = r;
                        melhorPos = pos;
                    }
                }
            }

            if (melhorRota < rotas.size())
            {
                rotas[melhorRota].insert(rotas[melhorRota].begin() + melhorPos,
                                         {1, s.id, s.origem, s.destino, s.custo});
                servicosUsados[s.id] = true;
                adicionado = true;
            }

            if (!adicionado)
            {
                Rota novaRota;
                novaRota.reserve(capacidadeVeiculo / 10 + 3);
                novaRota.emplace_back(0, 0, deposito, deposito, 0);
                novaRota.emplace_back(1, s.id, s.origem, s.destino, s.custo);
                novaRota.emplace_back(0, 0, deposito, deposito, 0);
                rotas.push_back(std::move(novaRota));
                servicosUsados[s.id] = true;
            }
        }

        return rotas;
    }

    // Construção gulosa randomizada com RCL adaptativa - MELHORIA 3
    std::vector<Rota> construcaoGulosaRandomizada(std::mt19937 &rng, const std::vector<Servico> &servicos)
    {
        std::vector<bool> servicosUsados(maxServicoId + 1, false);
        std::vector<Rota> rotas;

        size_t servicosRestantes = servicos.size();
        while (servicosRestantes > 0)
        {
            short capacidadeRestante = capacidadeVeiculo;
            Rota rota;
            rota.reserve(capacidadeVeiculo / 20 + 3);
            rota.emplace_back(0, 0, deposito, deposito, 0);

            while (capacidadeRestante > 0 && servicosRestantes > 0)
            {
                // RCL adaptativa baseada na qualidade dos candidatos
                std::vector<std::pair<double, const Servico *>> candidatos;
                candidatos.reserve(servicosRestantes);

                double melhorEficiencia = std::numeric_limits<double>::max();
                for (const auto &s : servicos)
                {
                    if (servicosUsados[s.id] || s.demanda > capacidadeRestante)
                        continue;
                    double eficiencia = (double)s.custo / std::max((short)1, s.demanda);
                    candidatos.emplace_back(eficiencia, &s);
                    melhorEficiencia = std::min(melhorEficiencia, eficiencia);
                }

                if (candidatos.empty())
                    break;

                std::sort(candidatos.begin(), candidatos.end());
                // RCL baseada em threshold ao invés de percentual fixo
                double threshold = melhorEficiencia * 1.2;
                int tamRCL = 0;
                for (auto &[eff, serv] : candidatos)
                {
                    if (eff <= threshold)
                        tamRCL++;
                    else
                        break;
                }
                tamRCL = std::max(1, std::min(tamRCL, 10));

                int escolhido_idx = std::uniform_int_distribution<int>(0, tamRCL - 1)(rng);
                const auto &escolhido = *candidatos[escolhido_idx].second;

                capacidadeRestante -= escolhido.demanda;
                rota.emplace_back(1, escolhido.id, escolhido.origem, escolhido.destino, escolhido.custo);
                servicosUsados[escolhido.id] = true;
                servicosRestantes--;
            }

            rota.emplace_back(0, 0, deposito, deposito, 0);
            if (rota.size() > 2)
            {
                rota.shrink_to_fit();
                rotas.push_back(std::move(rota));
            }
        }
        return rotas;
    }

    // GRASP com controle inteligente de alpha - MELHORIA 4
    std::vector<Rota> graspParalelo(const std::vector<Servico> &servicos, int maxIter = 50, int threadId = 0)
    {
        std::mt19937 rng(std::random_device{}() + threadId * 1000);
        std::vector<Rota> melhorLocal;
        int menorCustoLocal = 32767;
        int iterSemMelhora = 0;
        const int maxSemMelhora = 15;

        double alpha = 0.3;

        for (int iter = 0; iter < maxIter && iterSemMelhora < maxSemMelhora; ++iter)
        {
            int custoGlobal = melhorCustoGlobal.load(std::memory_order_acquire);

            // Ajuste mais agressivo baseado na convergência
            int gap = (menorCustoLocal - custoGlobal);
            if (gap < menorCustoLocal * 0.05) // Convergiu
            {
                alpha = std::min(alpha * 1.5, 0.8); // Mais diversificação
            }
            else if (gap > menorCustoLocal * 0.2) // Muito longe do ótimo
            {
                alpha = std::max(alpha * 0.7, 0.05); // Mais intensificação
            }

            std::vector<Rota> rotas;

            if (iter % 5 == 0)
            {
                rotas = construcaoGulosaSimplesEficiente(servicos);
            }
            else
            {
                rotas = construcaoGulosaRandomizada(rng, servicos);
            }

            // VNS com parada baseada em melhoria efetiva
            for (int vnsIter = 0; vnsIter < 5; ++vnsIter)
            {
                int custoAntesVNS = custoTotal(rotas);
                if (!vnsParalelo(rotas))
                    break;
                int custoDepoisVNS = custoTotal(rotas);

                // Se melhoria foi marginal, para
                if (custoAntesVNS - custoDepoisVNS < custoAntesVNS * 0.01)
                    break;
            }

            int custoAtual = custoTotal(rotas);
            if (custoAtual < menorCustoLocal)
            {
                melhorLocal = rotas;
                menorCustoLocal = custoAtual;
                iterSemMelhora = 0;
                alpha = std::max(alpha * 0.95, 0.1);

                atualizarMelhorSolucaoGlobal(rotas, custoAtual);
            }
            else
            {
                iterSemMelhora++;
                if (iterSemMelhora % 5 == 0)
                    alpha = std::min(alpha * 1.1, 0.5);
            }
        }

        return melhorLocal;
    }

    void atualizarMelhorSolucaoGlobal(const std::vector<Rota> &solucao, int custo)
    {
        int custoAtual = melhorCustoGlobal.load(std::memory_order_acquire);
        while (custo < custoAtual &&
               !melhorCustoGlobal.compare_exchange_weak(custoAtual, custo, std::memory_order_release))
        {
        }

        if (custo < custoAtual)
        {
            std::lock_guard<std::mutex> lock(melhorSolucaoMutex);
            if (custo < custoTotal(melhorSolucao))
            {
                melhorSolucao = solucao;
            }
        }
    }

public:
    void lerArquivo(const std::string &caminho)
    {
        std::ifstream arquivo(caminho);
        if (!arquivo.is_open())
        {
            std::cerr << "Erro ao abrir arquivo: " << caminho << std::endl;
            return;
        }

        std::string linha;
        short id_servico = 1;

        auto pos = caminho.find_last_of("/\\");
        std::string nomeBase = (pos == std::string::npos) ? caminho : caminho.substr(pos + 1);
        pos = nomeBase.find_last_of(".");
        this->nomeBase = (pos == std::string::npos) ? nomeBase : nomeBase.substr(0, pos);

        nosRequeridos.clear();
        arestasRequeridas.clear();
        arcosRequeridos.clear();

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
                distanciasCache.assign(qtdVertices + 1, std::vector<short>(qtdVertices + 1, INF));

                distanciasCalculadas.resize(qtdVertices + 1);
                distanciasMutex.resize(qtdVertices + 1);
                for (int i = 0; i <= qtdVertices; ++i)
                {
                    distanciasCalculadas[i] = new std::atomic<bool>(false);
                    distanciasMutex[i] = std::make_unique<std::mutex>();
                }
            }
            else if (linha.rfind("ReN.", 0) == 0)
            {
                while (std::getline(arquivo, linha) && !linha.empty() && linha[0] == 'N')
                {
                    std::stringstream ss(linha);
                    std::string id;
                    short demanda, custo;
                    ss >> id >> demanda >> custo;
                    short no = std::stoi(id.substr(1));
                    nosRequeridos.push_back({no, demanda, custo, id_servico++});
                }
            }
            else if (linha.rfind("ReE.", 0) == 0)
            {
                while (std::getline(arquivo, linha) && !linha.empty() && linha[0] == 'E')
                {
                    std::stringstream ss(linha);
                    std::string id;
                    short u, v, custo, demanda, scusto;
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
                    short u, v, custo, demanda, scusto;
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
                    short u, v, custo, dummy1, dummy2;
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
                    short u, v, custo, dummy1, dummy2;
                    ss >> id >> u >> v >> custo >> dummy1 >> dummy2;
                    grafo[u].emplace_back(v, custo);
                }
            }
        }

        construirMapas();
    }

    void construirRotas()
    {
        std::vector<Servico> todosServicos;
        todosServicos.reserve(nosRequeridos.size() + arestasRequeridas.size() + arcosRequeridos.size());

        for (auto &n : nosRequeridos)
            todosServicos.push_back({n.servico_id, n.id, n.id, n.custo, n.demanda});
        for (auto &e : arestasRequeridas)
            todosServicos.push_back({e.id, e.origem, e.destino, e.custo, e.demanda});
        for (auto &a : arcosRequeridos)
            todosServicos.push_back({a.id, a.origem, a.destino, a.custo, a.demanda});

        int nThreads = std::min(6, (int)std::thread::hardware_concurrency());
        std::vector<std::future<std::vector<Rota>>> futures;
        futures.reserve(nThreads);

        for (int t = 0; t < nThreads; ++t)
        {
            int maxIter = (t < nThreads / 2) ? 40 : 20;
            futures.push_back(std::async(std::launch::async, [&, t, maxIter]()
                                         { return graspParalelo(todosServicos, maxIter, t); }));
        }

        melhorSolucao.clear();
        int menorCusto = 32767;
        for (auto &f : futures)
        {
            auto sol = f.get();
            int custoSol = custoTotal(sol);
            if (custoSol < menorCusto)
            {
                menorCusto = custoSol;
                melhorSolucao = std::move(sol);
            }
        }
    }

    void salvarSolucao(long long clocks)
    {
        std::string nomeArquivo = "sol-" + nomeBase + ".dat";
        std::ofstream out(nomeArquivo);

        int custoTotalSol = custoTotal(melhorSolucao);
        out << custoTotalSol << "\n"
            << melhorSolucao.size() << "\n"
            << clocks << "\n"
            << clocks << "\n";

        short rota_id = 1;
        for (const auto &rota : melhorSolucao)
        {
            short demanda = 0, custo = 0;
            for (const auto &[tipo, id, u, v, c] : rota)
            {
                if (tipo == 1)
                {
                    demanda += mapaDemanda[id];
                    custo += mapaCusto[id];
                }
            }
            out << "0 1 " << rota_id++ << " " << demanda << " " << custo << " " << rota.size();
            for (const auto &[tipo, id, u, v, _] : rota)
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
    SetConsoleOutputCP(CP_UTF8);
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

    std::cout << "Todas as soluções foram geradas com sucesso!\n";
    return 0;
}
