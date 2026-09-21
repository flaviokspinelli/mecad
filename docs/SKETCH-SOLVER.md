# Decisão de arquitetura: domínio de sketch e primeiro solver

Estado: núcleo afim com integração de contorno simples em desenvolvimento, por
constraintSystem no documento .mcad v2. Data: 21/09/2026. Responsável: Codex.
Itens relacionados: PROD-03/04, SK-01/02/04. Nenhum aceite completo declarado.

## Investigação de motores

- SolveSpace oferece uma interface de biblioteca; sua página oficial identifica
  a distribuição como GPLv3 e orienta contato para outras condições. Não foi
  incorporado nem houve contato ou aquisição de licença.
  [Fonte oficial](https://solvespace.com/library.pl).
- PlaneGCS, no FreeCAD, expõe diferentes algoritmos não lineares e diagnóstico;
  seu cabeçalho atual contém SPDX LGPL-2.1-or-later e usa Eigen, SubSystem e
  definições de exportação do Sketcher. Integrá-lo exige fixar revisão, auditar
  o conjunto de arquivos/dependências e verificar a separação do FreeCAD.
  [Cabeçalho oficial](https://github.com/FreeCAD/FreeCAD/blob/main/src/Mod/Sketcher/App/planegcs/GCS.h).

Esta investigação não conclui compatibilidade jurídica de uma distribuição nem
benchmark comparativo. Nenhum código desses motores foi copiado para o projeto.
PROD-04 continua pendente para o inventário e auditoria completos.

## Decisão interna atual

Criar um domínio de sketch independente de Qt Widgets e Open CASCADE, baseado em
IDs explícitos, não em índices de faces/arestas. O solver inicial próprio resolve
somente relações afins: coincidência, horizontal, vertical, ponto fixo e diferenças
assinadas em X/Y. Não se trata de corrigir retângulos caso a caso: todas as relações
geram linhas de um sistema A*x=b, independente da figura.

Usa Gram-Schmidt modificado com reortogonalização para obter linhas independentes
e projetar a geometria inicial na solução de menor deslocamento euclidiano.
Graus de liberdade são número de coordenadas menos posto. A tolerância final é
absoluta, 1e-8 mm, verificada nas equações originais. Entrada fora de ±1e6 mm,
não finita, com referências inválidas ou dados desconhecidos é recusada.

O limite inicial é 128 pontos, 256 linhas e 256 restrições. Não é uma declaração
de desempenho de sketches grandes. Restrições redundantes são reportadas quando
nenhuma de suas equações acrescenta posto; redundância parcial não é reportada.
Em conflito, reporta contribuintes candidatos, não um conjunto mínimo garantido.
Não retorna coordenadas parcialmente resolvidas. solved() produz um novo documento
apenas quando a solução satisfaz a tolerância, preservando todos os IDs.

## Portas ainda obrigatórias antes da integração

1. Validar geometria após resolver: coincidência pode colapsar uma linha; convergência
   algébrica não implica contorno CAD válido, fechado ou sem auto-interseção.
2. Completar edição/arraste, cotas e representação de restrições na interface.
   Horizontal, vertical, fixação e remoção estão em integração; coincidência não
   é oferecida para o contorno simples porque fundir pontos exige editar a topologia.
3. Validar a migração do formato nativo. MecaCADSketch v1 é o formato embutido
   em constraintSystem de .mcad v2; a versão do sistema não é a do documento.
   Projetos sem restrições continuam v1. Versões anteriores devem recusar v2.
4. Implementar curvas e relações não lineares por um backend adequado; o motor
   afim atual não suporta comprimentos gerais, ângulos, tangência ou raios.
5. Validar solver não linear, diagnóstico visual e produtividade antes de afirmar
   sketch paramétrico profissional. Não substituir essa etapa por ajustes isolados.

## Testes

Executar `sh scripts/check.sh sketch`: testes sem janela, reutilizando build/.
Cobrem solução de menor deslocamento, graus de liberdade, retângulo restrito,
edição de cota, IDs, JSON em memória, entradas inválidas, conflito versus redundância,
ordem das equações, escala, componentes independentes e cadeia no limite de pontos.
Não geram versões do aplicativo ou arquivos de projeto do usuário.
