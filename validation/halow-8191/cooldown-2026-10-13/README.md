# Correção da janela receptora — 13/10/2026

O UdpEchoClient possui agora o atributo SendUntil: instante absoluto e exclusivo
para gerar pedidos. O padrão -1 desativa o limite e preserva o comportamento
dos demais usuários desse modelo. Send verifica o limite antes de criar o
pacote, chamar o trace Tx ou incrementar o contador. Um evento já agendado no
limite ou depois dele encerra sem transmitir ou agendar novos pedidos.

No scratch/test, após todas associarem, os clientes UDP Echo recebem SendUntil
igual ao instante atual mais a janela de tráfego. StopTime permanece relativo
à inicialização, conforme esta versão de Application, e passa a incluir o
cooldown apenas para esses clientes. Outros tipos de aplicação conservam seu
StopTime anterior. Não foram alterados parâmetros de rádio, RAW, TIM ou filas.

O teste scratch/udp-echo-cooldown-check.cc utiliza dois nós ligados por um enlace
com 200 ms de atraso por sentido. Há potencial para 100 pedidos, separados por
100 ms, com início em 1 s e limite em 1,3 s. Verifica:

- Comportamento padrão: 3 envios e nenhuma resposta antes de fechar o socket.
- Limite SendUntil e receptor até 2 s: exatamente 3 envios e 3 respostas tardias.
- Início exatamente em SendUntil: zero envios e zero respostas.

O teste retorna 1 se qualquer contagem ou limite temporal falhar. Ele não
substitui o controle de integração HaLow nem prova recuperação das 30 respostas
na execução8191. Execução deixada para o usuário; não foi iniciada simulação.

Da raiz do repositório, executar primeiro:

```bash
docker run --rm \
  -v "$PWD:/workspace/IEEE-802.11ah-ns-3" \
  -w /workspace/IEEE-802.11ah-ns-3 ns3-halow-env \
  env LD_LIBRARY_PATH=build-o2 ./build-o2/scratch/udp-echo-cooldown-check
```

Após conferir o teste dirigido, repetir controle32 usando run_rotating.py com
os mesmos parâmetros anteriores. Espera-se testar a recuperação do eco tardio
da STA4, sem presumir o resultado. Somente depois avaliar controle2048/8191.

O diagnóstico dos 30 pedidos não recebidos no AP continua pendente e não é
coberto por esta correção. A execução8191 anterior e seus hashes permanecem
como referência do código anterior.
