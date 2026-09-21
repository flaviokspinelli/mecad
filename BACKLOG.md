# Backlog mestre — MecaCAD profissional

Atualização: 21/09/2026. Base: MecaCAD 0.2.25, macOS Apple Silicon.

Progresso após essa versão: [registro de execução](docs/PROGRESSO.md).
UX-05: [pivô de rotação configurável](docs/ROTATION-PIVOT.md) implementado e
testado em desenvolvimento; aceite integral ainda pendente.
Pedido vigente em 21/09/2026: **terminar os 38 itens parciais**, sem ampliar o
escopo. Substitui as ampliações anteriores de 65/95 itens. A lista fica congelada;
um novo avanço não adiciona outro item automaticamente. Lista dos 38:

- PROD-03, PROD-05;
- REL-01, REL-02, REL-03, REL-04, REL-06, REL-07;
- UX-01, UX-03, UX-04, UX-05, UX-06, UX-07, UX-08;
- SK-01, SK-02, SK-03, SK-04, SK-05, SK-11;
- PAR-01, PAR-02, PAR-03, PAR-04, PAR-05, PAR-06;
- GEO-01, GEO-02, GEO-03, GEO-04, GEO-08, GEO-13;
- FAB-01, FAB-05; QA-01, QA-02, QA-05.

Sequência interna: integridade e parâmetros; seleção/sketch/referências;
modelagem; componentes/montagens; desenhos e malhas. Dependências indispensáveis
continuam autorizadas.

Os outros 63 itens continuam no backlog geral, mas não nesta entrega, salvo
dependência indispensável ao aceite dos 38. Não há autorização para compra de
licenças, serviços externos ou execução de máquinas físicas.

Fechar significa cumprir o aceite integral de cada linha abaixo, não apenas
implementar parte dela. Restrições de disco e de testes permanecem em vigor.

Política de testes: grupos direcionados e sem janelas durante o desenvolvimento;
interface somente quando necessária, com aviso; suíte completa antes da entrega consolidada.

**Restrição de armazenamento (21/09/2026):** não gerar novos arquivos de versões,
pacotes .app versionados, ZIP/DMG/PKG ou cópias temporárias de empacotamento.
Reutilizar apenas build/ para desenvolvimento e testes. Preservar a distribuição
atual dist/MecaCAD-0.2.25.app. O empacotador está desativado; nova geração ou
substituição da distribuição depende de autorização explícita do usuário,
inclusive na entrega consolidada. Esta restrição prevalece sobre as instruções
de empacotamento anteriores deste documento.

## Objetivo e compromisso de qualidade

Desenvolver um CAD/CAM/CAE paramétrico completo para uso interno em mecatrônica,
com organização e interação familiares a quem usa Fusion, produtividade comparável
nos fluxos definidos e confiabilidade para preservar os projetos.

Este é um backlog de produto completo, não de MVP. Os marcos organizam o trabalho
interno; não exigem que o usuário teste versões intermediárias. Desenvolver,
reproduzir falhas, testar pela interface e validar entregas é responsabilidade
do desenvolvimento. Entregar pacotes consolidados com resultados e limitações.

“Tão bom quanto o Fusion” exige duas dimensões: cobertura funcional e qualidade
de execução. Ter um botão ou uma operação geométrica não conclui a funcionalidade.
Não prometer equivalência percentual, prazos ou certificação sem evidências.

Este documento não é um inventário verificado de cada recurso da versão atual
do Fusion. É o escopo-alvo do MecaCAD. A comparação formal de capacidades e
comportamentos faz parte de PROD-01 e deve usar uma versão de referência definida.

## Estados, prioridade e rastreamento

- **P0:** bloqueador de confiança, integridade ou modelagem paramétrica diária.
- **P1:** capacidade central de um CAD profissional produtivo.
- **P2:** capacidade necessária à visão completa de mecatrônica/CAM/CAE.
- **P3:** expansão avançada; permanece no backlog completo, não está descartada.
- **Parcial:** há uma base, mas o aceite do item ainda não foi cumprido.
- **Pendente:** não entregue nesse escopo.
- **Concluído:** aceite implementado e verificado, com versão e evidência registradas.

As prioridades definem sequência, não exclusão do escopo. Nenhum item está
automaticamente em execução. Ao iniciar um item, registrar responsável, dependências,
risco, casos de teste e evidências; ao concluir, registrar versão/commit e limitações.
Estimativas só após investigação técnica; não há prazo fechado neste documento.

## Base existente e dívida conhecida

A versão 0.2.25 usa C++20, Qt e Open CASCADE. Há viewport, cubo de orientação,
seleção de corpos/subelementos/perfis, Shift e área, primitivas, sketches básicos,
cotas de retângulos/círculos, snapping, extrusão/corte/revolução, booleanas, furo,
filete, movimento/rotação, histórico, desfazer/refazer, arquivo nativo, STEP/STL
e exportação básica de sketch em DXF.

Isso não representa maturidade profissional: falta solver geral de sketch,
regiões compostas, referências persistentes robustas e processamento assíncrono.
Os vínculos atuais usam índices topológicos; modelos grandes não foram qualificados.
STL continua sendo malha, não CAD paramétrico.

Evidências: [qualidade da 0.2.25](docs/QUALIDADE-0.2.25.md).
Histórico: [entrega 0.1](docs/ENTREGA-0.1.md), [entrega 0.2](docs/ENTREGA-0.2.md).

## 1. Referência de produto e arquitetura

| ID | Prioridade / estado | Trabalho | Aceite verificável |
| --- | --- | --- | --- |
| PROD-01 | P0 · Pendente | Matriz de comparação com Fusion | Fixar versão, plataforma e fluxos de referência; registrar comandos, gestos, resultados e lacunas, sem confundir semelhança visual com equivalência. |
| PROD-02 | P0 · Pendente | Metas de usabilidade e desempenho | Medir tarefas iguais no hardware de referência: tempo, ações, erros, latência e travamentos; fixar metas antes de declarar paridade. |
| PROD-03 | P0 · Parcial | Arquitetura modular | Separar documento, geometria, solver, comandos, seleção, renderização e arquivos; comandos testáveis sem janela e limites claros entre módulos. |
| PROD-04 | P0 · Pendente | Dependências e licenças | Registrar componentes, versões, licenças e obrigações de distribuição antes de incorporar solver, malhador ou CAM. |
| PROD-05 | P1 · Parcial | Configuração e documentação | Preferências persistentes de unidades, navegação, idioma e atalhos; ajuda contextual compatível com recursos realmente implementados. |
| PROD-06 | P2 · Pendente | Portabilidade | Builds e testes próprios para Windows/Linux; manter macOS como plataforma atual, sem presumir migração já aprovada. |

## 2. Integridade, arquivos e confiabilidade

| ID | Prioridade / estado | Trabalho | Aceite verificável |
| --- | --- | --- | --- |
| REL-01 | P0 · Parcial | Comandos transacionais | Prévia não altera documento; confirmar gera uma etapa; cancelar, falhar ou consultar seleção não altera parâmetros/histórico. |
| REL-02 | P0 · Parcial | Undo/redo completo | Reverter e reaplicar modelagem, seleção de referências, parâmetros e operações em lote sem estados divergentes; gesto inteiro vira uma ação. |
| REL-03 | P0 · Parcial | Recuperação automática | Simular encerramento forçado e recuperar alterações; não sobrescrever original nem recuperação de outro documento; explicar o que foi recuperado. |
| REL-04 | P0 · Parcial | Formato nativo versionado | Migrações verificadas com arquivos de cada versão publicada; recusar formatos incompatíveis sem perda; salvar atomicamente. |
| REL-05 | P0 · Pendente | Operações assíncronas e canceláveis | Importar/reconstruir/simular sem bloquear UI; progresso, cancelamento e proteção contra resultados obsoletos ou concorrentes. |
| REL-06 | P0 · Parcial | CI e pacote reproduzível | Executar geometria/UI/arquivos automaticamente; guardar evidências; bloquear empacotamento com regressões; verificar instalação limpa. |
| REL-07 | P1 · Parcial | Diagnósticos e suporte | Erro identifica etapa, causa provável e recuperação; log local exportável sem incluir projetos privados automaticamente. |
| REL-08 | P1 · Pendente | Documentos grandes | Reconstrução incremental, cache, renderização otimizada e limites explícitos; medir memória/latência com conjuntos pequenos, médios e grandes. |
| REL-09 | P1 · Pendente | Revisões e múltiplos documentos | Abrir vários projetos, recuperar cada um e comparar/restaurar revisões sem sobrescrita acidental; separar revisão de arquivo de histórico geométrico. |

## 3. Interface e interação direta

| ID | Prioridade / estado | Trabalho | Aceite verificável |
| --- | --- | --- | --- |
| UX-01 | P0 · Parcial | Seleção consistente | Hover e clique concordam sobre face/aresta/vértice/corpo/perfil em vários zooms; Shift alterna seleção; filtro ativo sempre perceptível. |
| UX-02 | P0 · Pendente | Escolha de alvos sobrepostos | Lista/ciclo de candidatos com realce; acessar sketches sem esconder corpos; profundidade e intenção previsíveis. |
| UX-03 | P0 · Parcial | Seleção por área | Distinguir conter/cruzar e visível/através; testar sentidos do arraste, Shift e filtros sem capturar gestos de desenho. |
| UX-04 | P0 · Parcial | Convenções universais | Enter confirma, Esc cancela, Delete apaga, rollback é separado; atalhos não interferem com campos de texto; pan/órbita não criam geometria. |
| UX-05 | P0 · Parcial | Manipuladores completos | Translação livre, por eixo/plano, rotação nos três eixos e pivô reposicionável; prévia contínua e precisão numérica para CAD e STL. |
| UX-06 | P1 · Parcial | Edição dentro do comando | Acrescentar/remover alvos sem fechar painel; prévia acompanha seleção, unidades e parâmetros; inputs no canvas quando apropriado. |
| UX-07 | P1 · Parcial | Contexto e descoberta | Menus contextuais, menu radial, busca e atalhos configuráveis; comandos indisponíveis explicam o motivo, não simulam capacidade inexistente. |
| UX-08 | P1 · Parcial | Ergonomia e acessibilidade | Testar mouse/trackpad, Retina, contraste, escala de texto, teclado e painéis; idioma/unidades consistentes; nenhuma ação essencial fica encoberta. |
| UX-09 | P1 · Pendente | Personalização do ambiente | Salvar layouts, favoritos e presets de navegação; restaurar padrão sem perder projeto; onboarding opcional baseado em tarefas. |

## 4. Sketch paramétrico profissional

Investigar solver, licença e estabilidade numérica antes de SK-02.
Não substituir o solver por correções específicas de cada figura.

| ID | Prioridade / estado | Trabalho | Aceite verificável |
| --- | --- | --- | --- |
| SK-01 | P0 · Parcial | Entidades persistentes num sketch | Múltiplas linhas/curvas e relações com IDs estáveis; editar uma entidade não recria arbitrariamente as demais. |
| SK-02 | P0 · Parcial | Solver de restrições básicas | Coincidente, horizontal, vertical e fixação com convergência/tolerância testadas; solução inválida não destrói o estado anterior. Núcleo afim interno testado; integração com documento/UI ainda pendente. |
| SK-03 | P0 · Parcial | Cotas gerais no canvas | Comprimentos, distâncias, ângulos, raio/diâmetro; cotas controladoras e de referência; edição direta e decimal local. |
| SK-04 | P0 · Parcial | Graus de liberdade e conflitos | Identificar sub-restrito, totalmente restrito e sobre-restrito; realçar relações conflitantes e permitir reparo. Diagnóstico afim interno testado; realce e reparo na UI ainda pendentes. |
| SK-05 | P0 · Parcial | Arraste com restrições | Arrastar pontos/linhas/curvas mantendo relações; solução em tempo real e undo de um único gesto. |
| SK-06 | P0 · Pendente | Regiões e contornos compostos | Reconhecer conectividade, ilhas, furos e sobreposições; selecionar região com prévia inequívoca para extrusão. |
| SK-07 | P1 · Pendente | Restrições avançadas | Paralela, perpendicular, tangente, igual, concêntrica, simétrica e ponto médio; diagnosticar redundância e incompatibilidade. |
| SK-08 | P1 · Pendente | Trim/extend/split | Aparar, estender e dividir linhas/arcos/círculos com prévia do trecho; preservar relações válidas e explicar relações removidas. |
| SK-09 | P1 · Pendente | Offset, espelho e padrões 2D | Cópias associativas editáveis; tratar auto-interseções, colapso de offset e quantidades inválidas. |
| SK-10 | P1 · Pendente | Projeção e construção | Projetar geometria com vínculo ao suporte; linhas de construção não geram regiões; referências rompidas ficam identificadas. |
| SK-11 | P1 · Parcial | Snapping e inferências | Extremidade, centro, meio, interseção e alinhamento; feedback visual e distinção entre snap temporário e restrição permanente. Interseções entre segmentos, círculos e arcos integradas e testadas nos três planos; limites e evidências em docs/SNAPPING.md. |
| SK-12 | P1 · Pendente | Geometria de sketch ampliada | Slots, retângulos orientados, elipses, splines e texto; edição por alças/cotas, fontes previsíveis e contornos válidos. |
| SK-13 | P2 · Pendente | Sketch 3D | Entidades espaciais, projeções e restrições compatíveis; caminhos utilizáveis em sweep sem saltos de coordenadas. |

## 5. Histórico, parâmetros e referências

PAR-01/02 sustentam projeções, recursos avançados e montagens associativas.

| ID | Prioridade / estado | Trabalho | Aceite verificável |
| --- | --- | --- | --- |
| PAR-01 | P0 · Parcial | Identificação persistente de topologia | Preservar intenção após mudanças testadas em faces/arestas; ambiguidade pede reparo, nunca aponta silenciosamente para outra entidade. |
| PAR-02 | P0 · Parcial | Grafo explícito de dependências | Detectar ciclos e entradas inválidas; reconstruir na ordem correta; falha não corrompe o último estado válido. |
| PAR-03 | P0 · Parcial | Diagnóstico e reparo do histórico | Identificar etapa quebrada e dependentes; substituir referência/perfil; separar excluir, suprimir e rollback. |
| PAR-04 | P1 · Parcial | Parâmetros e expressões | Nomes, fórmulas e unidades reutilizáveis; validar ciclos, tipos e expressões sem execução arbitrária de código. Tabela, vínculos de comprimentos/ângulos e cotas X/Y do solver integrados; cotas angulares/radiais do solver dependem da ampliação de SK-03. |
| PAR-05 | P1 · Parcial — aceite funcional verificado; entrega pendente | Reordenar e suprimir recursos | Recusar ordem impossível; reativar etapas e manter estado após salvar/reabrir. Supressão direta/por dependência, reativação atômica, UI e persistência verificadas; falta a porta global de pacote consolidado. Ver docs/SUPPRESSION.md. |
| PAR-06 | P1 · Parcial | Editar recurso existente | Reabrir parâmetros/seleções originais sem duplicar peça; cancelar restaura integralmente; histórico e canvas permanecem coerentes. |
| PAR-07 | P2 · Pendente | Configurações de projeto | Variantes por parâmetros/supressões com identificação clara; exportar configuração escolhida e reconstruir cada variante. |

## 6. Modelagem sólida, superfícies e formas

| ID | Prioridade / estado | Trabalho | Aceite verificável |
| --- | --- | --- | --- |
| GEO-01 | P0 · Parcial | Extrusão/corte completos | Múltiplas regiões, duas direções/simetria, até face e passante; alvo explícito; prévia coincide com resultado; zero não gera operação. |
| GEO-02 | P1 · Parcial | Furo por clique | Centro cotado sobre face, profundidade/passante, rebaixo/escareado e padrões; edição preserva referência. |
| GEO-03 | P1 · Parcial | Filete completo | Seleção dinâmica, cadeias tangentes, edição de raio e diagnóstico de arestas problemáticas; raios variáveis como extensão explicitamente testada. |
| GEO-04 | P1 · Parcial | Chanfro | Igual distância, duas distâncias e distância/ângulo; prévia, seleção e edição paramétrica. Três modos, edição e seleção dinâmica no painel integrados; referências topológicas robustas ainda pendentes. |
| GEO-05 | P1 · Pendente | Casca e inclinação | Faces removidas, espessura, plano neutro e direção; diagnosticar geometrias inviáveis sem perder o corpo original. |
| GEO-06 | P1 · Pendente | Espelho e padrões 3D | Corpos/recursos, padrões lineares/circulares e por caminho; quantidades editáveis e instâncias inválidas identificadas. |
| GEO-07 | P1 · Pendente | Loft e sweep | Perfis, guias, caminhos e continuidade; detectar auto-interseção; reconstruir após editar entradas. |
| GEO-08 | P1 · Parcial | Booleanas e divisão | Vários alvos/ferramentas, manter ferramentas opcionalmente, dividir por plano/superfície e nomear resultados. |
| GEO-09 | P1 · Pendente | Construção geométrica | Planos/eixos/pontos por offset, ângulo e referências; reutilizar em sketches, padrões e operações. |
| GEO-10 | P1 · Pendente | Superfícies | Criar patches, extrudar/revolver/loft/sweep de superfícies, aparar/estender/costurar e espessar; medir folgas e verificar fechamento. |
| GEO-11 | P2 · Pendente | Continuidade e formas livres | Controle de continuidade, curvatura e deformação por alças; converter em geometria utilizável e diagnosticar superfícies inválidas. |
| GEO-12 | P1 · Pendente | Edição direta e importados | Mover/offset/substituir/remover faces com reparo; preservar detalhes não afetados; distinguir edição direta de histórico paramétrico. |
| GEO-13 | P1 · Parcial | Medição e análise | Distância, ângulo, raio, área, volume, massa e centro de massa; seções e análise visual com unidades/material corretos. |
| GEO-14 | P2 · Pendente | Roscas e detalhes mecânicos | Roscas representadas ou modeladas, parâmetros normativos definidos, edição e representação coerente no desenho técnico. |

## 7. Montagens e mecatrônica

| ID | Prioridade / estado | Trabalho | Aceite verificável |
| --- | --- | --- | --- |
| ASM-01 | P1 · Pendente | Componentes e instâncias | Hierarquia, sistemas locais, corpos/sketches por componente; mover instância sem deformar sua definição. |
| ASM-02 | P1 · Pendente | Submontagens e referências externas | Inserir/copiar/vincular com distinção explícita; atualizar versão, resolver arquivo ausente e empacotar dependências. |
| ASM-03 | P1 · Pendente | Juntas mecânicas | Fixa, rotativa, linear e demais graus de liberdade definidos; origens/alinhamentos selecionáveis e movimento verificável. |
| ASM-04 | P2 · Pendente | Limites e relações de movimento | Limites, acoplamentos e posições salvas; movimento respeita relações e informa incompatibilidades. |
| ASM-05 | P1 · Pendente | Interferência e folga | Identificar pares e volume de colisão; medir folgas em posições escolhidas e produzir relatório. |
| ASM-06 | P2 · Pendente | Lista de materiais | Código, descrição, quantidade, material e revisão por instância; exportar CSV e associar chamadas no desenho. |
| ASM-07 | P2 · Pendente | Biblioteca de componentes | Motores, rolamentos, parafusos, sensores e conectores com dimensões, origem e licença; permitir biblioteca interna. |

## 8. Desenho técnico, chapas e fabricação

| ID | Prioridade / estado | Trabalho | Aceite verificável |
| --- | --- | --- | --- |
| FAB-01 | P1 · Parcial | STEP/STL/DXF confiáveis | Validar unidades, orientação, curvas e precisão de malha; comparar dimensões após reabertura em ferramenta independente. |
| FAB-02 | P1 · Pendente | Folhas e vistas associativas | Formatos/carimbo/escala, vistas ortográficas/isométricas; atualização após mudar o modelo e alerta de vista desatualizada. |
| FAB-03 | P1 · Pendente | Cortes e detalhes | Cortes, detalhes ampliados, hachuras e vistas interrompidas com escala e posicionamento editáveis. |
| FAB-04 | P1 · Pendente | Cotagem técnica | Cotas, tolerâncias, centros, acabamento, GD&T e chamadas conforme convenção escolhida; referências persistentes e layout legível. |
| FAB-05 | P1 · Parcial | PDF/DXF de desenho | Exportar folhas em escala, textos/layers/curvas íntegros; validar impressão e reabertura. PDF técnico ainda não existe. |
| FAB-06 | P1 · Pendente | Chapas paramétricas | Espessura, flanges, dobras, alívios e regras de material; parâmetros coerentes após editar dimensões. |
| FAB-07 | P1 · Pendente | Planificação | Fator de dobra configurável, abrir/refazer dobras, DXF de corte e conferência com peças de referência. |
| FAB-08 | P2 · Pendente | Plásticos e encaixes | Nervuras, ressaltos, encaixes, folgas e análises de espessura/inclinação verificáveis. |
| FAB-09 | P2 · Pendente | Impressão 3D e malhas | Detectar malhas abertas/invertidas, reparar com prévia, simplificar e controlar conversão quando viável; não prometer recuperar parâmetros de STL. |

## 9. CAM e manufatura assistida

Depende de sólidos válidos, REL-05 e validação específica de máquinas.
Simulação visual não é autorização para executar código numa máquina real.

| ID | Prioridade / estado | Trabalho | Aceite verificável |
| --- | --- | --- | --- |
| CAM-01 | P2 · Pendente | Setup de fabricação | Estoque, fixação, zero-peça, eixos e máquina; unidades/orientação explícitas e persistentes. |
| CAM-02 | P2 · Pendente | Biblioteca de ferramentas | Geometria, porta-ferramenta e parâmetros de corte com origem; verificar compatibilidade com máquina/operação. |
| CAM-03 | P2 · Pendente | Percursos 2D/2,5D | Faceamento, contorno, bolsão e furação; entradas/saídas, alturas seguras e estoque remanescente configuráveis. |
| CAM-04 | P2 · Pendente | Percursos 3 eixos | Desbaste/acabamento de superfícies, tolerância e passo lateral; verificar material residual e colisões. |
| CAM-05 | P2 · Pendente | Simulação de usinagem | Remoção de material, colisão com ferramenta/porta-ferramenta/fixação, movimentos rápidos e limites de máquina. |
| CAM-06 | P2 · Pendente | Pós-processadores e folhas de processo | Código para controladores definidos, testes de referência e revisão; saída inclui ferramenta, setup e avisos. |
| CAM-07 | P3 · Pendente | Torneamento e multieixos | Escopo por tipo de máquina, cinemática/colisões e pós-processador validado separadamente; sem suporte genérico implícito. |
| CAM-08 | P3 · Pendente | Nesting e fabricação aditiva | Arranjo para corte e preparação de trajetórias aditivas com formatos/máquinas definidos e validação própria. |

## 10. Simulação e análise de engenharia

Resultados precisam de validação numérica, não apenas mapas coloridos.

| ID | Prioridade / estado | Trabalho | Aceite verificável |
| --- | --- | --- | --- |
| SIM-01 | P2 · Pendente | Materiais e estudos | Propriedades com unidades/proveniência, hipóteses e condições ambientais registradas; estudos isolados da geometria original. |
| SIM-02 | P2 · Pendente | Malha e condições de contorno | Inspecionar qualidade, refinar localmente, aplicar cargas/apoios/contatos e diagnosticar graus de liberdade não restringidos. |
| SIM-03 | P2 · Pendente | Estrutural estática | Integrar solver licenciado adequadamente; tensões/deslocamentos/reações e cancelamento; comparar com soluções conhecidas. |
| SIM-04 | P2 · Pendente | Resultados e convergência | Escala de deformação explícita, convergência de malha, unidades, fatores de segurança e relatório reproduzível. |
| SIM-05 | P2 · Pendente | Térmica e modal | Condução/condições térmicas e modos naturais em estudos separados, com referências analíticas ou benchmarks. |
| SIM-06 | P3 · Pendente | Flambagem, não linear e contatos avançados | Documentar hipóteses e limites; testes específicos de convergência e comparação independente por tipo de estudo. |
| SIM-07 | P3 · Pendente | Dinâmica, fluidos e otimização | Estudos de viabilidade separados; definir domínio físico, custo, solver e validação antes da implementação. |

## 11. Eletrônica, visualização e extensibilidade

| ID | Prioridade / estado | Trabalho | Aceite verificável |
| --- | --- | --- | --- |
| ELE-01 | P2 · Pendente | Integração mecânica de PCB | Importar placa/componentes com sistemas de coordenadas e envelopes; conferir furos, conectores e colisões no gabinete. |
| ELE-02 | P3 · Pendente | Esquemático e PCB integrados | Investigar motor próprio ou integração; nets, bibliotecas, regras elétricas/layout, roteamento e arquivos de fabricação verificáveis. |
| VIS-01 | P2 · Pendente | Materiais e renderização | Aparência, iluminação, câmeras e exportação de imagens; aparência visual não altera propriedades físicas sem escolha explícita. |
| VIS-02 | P2 · Pendente | Explodidos e animação | Sequência de montagem, trajetórias e exportação; não modificar posições de projeto ao reproduzir animação. |
| API-01 | P2 · Pendente | API e automação | Comandos/documentos acessíveis por API versionada, transações e exemplos; scripts não contornam validação do modelo. |
| DATA-01 | P3 · Pendente | Colaboração e revisão compartilhada | Permissões, conflitos, revisões e recuperação offline; dependência de nuvem opcional e aprovada, não obrigatória para uso interno. |

## 12. Qualidade de produto e validação

| ID | Prioridade / estado | Trabalho | Aceite verificável |
| --- | --- | --- | --- |
| QA-01 | P0 · Parcial | Regressões encadeadas | Projetos, ações, dimensões/volumes esperados e capturas reproduzíveis, sem pedir ao usuário descobrir defeitos. |
| QA-02 | P0 · Parcial | Teste real de interface | Exercitar mouse/teclado e painéis, não apenas API geométrica; inspecionar prévia, alvo selecionado e resultado final. |
| QA-03 | P0 · Pendente | Desempenho e longa duração | Medir abertura/seleção/reconstrução e memória em tamanhos definidos; executar sessões prolongadas e investigar degradação. |
| QA-04 | P0 · Pendente | Injeção de falhas | Cancelar, simular falha de gravação, importar arquivos inválidos e interromper processo; recuperar sem corrupção silenciosa. |
| QA-05 | P1 · Parcial | Corpus de mecatrônica | Peças sintéticas desde o início; projetos reais somente quando disponibilizados/autorizados; registrar cobertura e defeitos. Suporte, caixa e espaçador sintéticos com volumes analíticos; ampliar diversidade e fluxos. |
| QA-06 | P1 · Pendente | Comparação de produtividade | Repetir tarefas de PROD-01 em ambos os produtos; divulgar resultados, metodologia e limites, sem alegar paridade universal. |

## Sequência e dependências de execução

1. **Fundação:** PROD-01/02/03/04, REL-01/02/03/04/06 e QA-01/02.
2. **Núcleo paramétrico:** SK-01/02 e PAR-01/02. Investigar solver e identificação
   topológica antes de ampliar recursos que dependam deles. REL-05 sustenta prévias
   e cálculos demorados; sua implementação exige política de concorrência.
3. **Modelagem diária:** SK-03/04/05/06/07, UX-01/02/03/04, GEO-01 e PAR-03.
4. **Produtividade profissional:** demais ferramentas de sketch, sólidos e
   superfícies P1, parâmetros/fórmulas, desenho técnico, chapas e componentes.
5. **Produto integrado:** montagens completas, bibliotecas, integração PCB,
   renderização, CAM inicial e simulação validada. CAM e CAE têm portas de qualidade próprias.
6. **Cobertura avançada:** configurações, formas livres, automação, eletrônica
   completa, análises avançadas, multieixos e colaboração opcional.

Não iniciar recursos avançados apenas para aumentar contagem de funcionalidades
enquanto persistirem falhas de integridade ou seleção no caminho principal.

## Cenários obrigatórios de aceite integrado

- **Suporte de motor:** sketch cotado/restrito → extrusão → furos/corte → filetes
  selecionados → alterar espessura/largura → reconstruir → salvar/reabrir → STEP/STL.
- **Caixa com tampa:** casca → encaixes e ressaltos → folga por expressão → padrões
  → alterar dimensões → conferir interferência → desenho cotado.
- **Perfil composto:** entidades sobrepostas → trim → selecionar regiões/ilhas
  → extrudar mantendo vazios → alterar contorno sem trocar alvos silenciosamente.
- **Peça de chapa:** flanges/alívios/dobras → alterar espessura → planificar
  → conferir dimensões → exportar DXF com escala/unidades corretas.
- **Montagem:** motor/eixo/rolamentos/suporte → juntas/limites → testar posições
  → detectar colisões → lista de materiais e vista explodida.
- **Histórico quebrado:** alterar/remover referência → diagnóstico → reparo
  → undo/redo → salvar/reabrir; cancelar outra operação deixa documento idêntico.
- **Intercâmbio:** STEP/STL → selecionar/mover/girar → salvar/reabrir/exportar
  → comparar dimensões externamente; limites de malha são explícitos.
- **CAM:** setup/estoque/fixação → ferramenta/percurso → simular colisões
  → gerar saída para controlador definido e revisar independentemente.
- **CAE:** material/malha/apoios/carga → resolver benchmark → refinar malha
  → comparar erro e convergência → emitir relatório com hipóteses.
- **PCB/gabinete:** inserir placa e conectores → verificar folgas/acessos/furos
  → atualizar revisão → identificar incompatibilidades mecânicas.

## Definição de pronto e portas de entrega

Um item só é concluído quando:

1. Cumpre o aceite com entradas válidas, inválidas e casos de fronteira.
2. Prévia, confirmar, cancelar e undo/redo são coerentes, quando aplicáveis.
3. Salvar/reabrir preserva resultados e referências; migrações são testadas.
4. A interface foi exercitada e inspecionada; compilar ou testar só o núcleo não basta.
5. Possui regressão automatizada, registro de execução e limitações documentadas.
6. O pacote foi verificado sem alterar ou apagar arquivos do usuário.

**Porta A — confiança:** todos os P0 aceitos, cenários aplicáveis encadeados,
recuperação comprovada e nenhum defeito conhecido de perda silenciosa de dados.

**Porta B — CAD profissional:** P1 aceitos, peças/sketches/montagens/desenhos/chapas
com fluxos consistentes, medições de desempenho e produtividade contra a referência.

**Porta C — produto completo no escopo:** P2 aceitos com validação específica de
CAM/CAE/eletrônica integrada; sem confundir implementação com certificação.

**Porta D — cobertura avançada:** P3 verificados por domínio. Declarar precisamente
o que é comparável, distinto ou não suportado na matriz PROD-01.

A passagem por uma porta não implica equivalência universal ao Fusion. Entregar
um pacote consolidado com matriz de aceites, resultados de teste e limites reais,
sem depender de o usuário validar correções básicas uma a uma.

## Decisões a registrar antes dos respectivos módulos

- Solver de sketch, estratégia de referências persistentes e modelo de concorrência.
- Convenções/unidades/normas de desenho, chapas, roscas e materiais.
- Máquinas/controladores de CAM e classes de estudo CAE prioritárias.
- Integração ou implementação própria de eletrônica e formatos de PCB.
- Política de revisões, colaboração opcional e plataformas adicionais.
- Licenças, orçamento e cronograma após os estudos de viabilidade.

Nenhuma decisão pendente acima bloqueia a correção da base atual. Formatos
proprietários nativos, certificação de resultados e suporte universal a máquinas
não são promessas implícitas deste backlog.
