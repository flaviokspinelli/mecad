# Execução do backlog

## 21/09/2026 — preferências locais de visualização e encaixe

PROD-05: fundo claro/escuro, arestas visíveis, snap e encaixe inteligente agora
persistem em preferences.ini, fora do documento. Valores ausentes/inválidos usam
defaults; falha de sync informa que a preferência vale somente na sessão. A
interface restaura a seleção dos menus e o estado do canvas ao construir a janela.
O caminho pode ser isolado nos testes; sessões com recoveryDirectory customizado
usam essa pasta por padrão, sem tocar preferências normais do usuário.

displayPreferencesPersistWithoutDocumentChanges e displayPreferenceWriteFailureIsReported
passaram offscreen (dois casos, quatro resultados Qt). Verificados reabrir,
fallback, escrita recusada, documento/dirty e undo independentes. MecaCAD e ui_tests
recompilados em build; nenhuma janela de teste ou nova distribuição criada.
Documentação em PREFERENCES.md. Idioma, unidades, atalhos e presets de navegação
ainda pendentes; não fecha PROD-05 nem altera contagem de itens completos.

## 21/09/2026 — busca de comandos com filtro em tempo real

UX-07: substituída a caixa de seleção por busca filtrável, sem distinção entre
maiúsculas/acentos e com múltiplos termos em qualquer ordem. Resultados mostram
ícones e atalhos; ↑/↓ navegam, Enter executa após fechar o painel, Esc cancela.
Sem resultados não executa; ações desabilitadas são indicadas e bloqueadas.
Não depende de texto digitado ser exatamente igual ao nome completo da ação.

commandSearchFiltersAndKeyboard e inlineProfileExpressionsAndUnits passaram
(dois casos funcionais, quatro resultados Qt). Busca valida documento inalterado
ao cancelar, ação disparada uma vez e ações desabilitadas bloqueadas. Inspecionada
build/command-search.png; MecaCAD recompilado sem pacote ou nova versão distribuída.

Limites em COMMAND-SEARCH.md: não há sinônimos, favoritos ou atalhos configuráveis;
exigências geométricas de ações habilitadas continuam validadas pelo próprio
comando. UX-07 segue parcial, sem mudança artificial na contagem do backlog.

## 21/09/2026 — mobilidade dos pontos no sketch

SK-04: solver fornece o grau de liberdade local de cada ponto pela projeção do
espaço livre sobre suas coordenadas. Detecta fixação indireta por cotas/relações,
não apenas relação Fixed. Mobilidades locais não são aditivas (movimento acoplado
é compartilhado). Soluções incompatíveis não retornam classificação parcial.

Canvas de sketches com constraintSystem mostra pontos presos em dourado e móveis
em azul, legenda e grau de liberdade global. Diagnóstico é armazenado no refresh,
sem solver por quadro, e invalidado junto com a atualização de geometria.
Não modifica arquivo nativo nem adiciona novos gestos de arraste.

Nove casos do solver passaram (11 resultados Qt); quatro casos de UI passaram
(seis resultados Qt): sketchMobilityUpdatesAfterUndo, reviewRedundantSketchConstraints,
conflictingSketchConstraintIsExplained e namedSketchDimensionFromSelection.
Inspecionada build/sketch-mobility.png: pontos presos/móveis distintos. Build
existente recompilado, dist e arquivos do usuário preservados. Classificação não
cobre curvas fora do solver atual; SK-04 continua parcial.

## 21/09/2026 — unidades e fórmulas nas cotas dos perfis

SK-03/PAR-04: largura/altura de retângulo e diâmetro de círculo no canvas aceitam
unidades e parâmetros nomeados. Círculo converte a expressão do diâmetro para raio
interno e reabre mostrando a medida exibida. Enter sem alteração não regrava nem
acumula conversões. Números simples sem vínculo mantêm a edição numérica anterior.
Campo vazio, unidade angular em comprimento e valores inválidos preservam o estado.

inlineProfileExpressionsAndUnits, inlineDimensions e namedSketchDimensionFromSelection
passaram (três casos, cinco resultados Qt). Cobrem vírgula decimal, cm, atualização
de parâmetro, undo/redo, persistência, erro e cancelamento. A regressão inicialmente
detectou leitura com QJsonObject::operator[] inserindo campo expressions vazio;
corrigida para value(), com teste explícito de abrir/cancelar sem alterar documento.

MecaCAD recompilado no build existente, sem distribuição nova. Não implementa
cotas angulares ou restrições radiais gerais no solver, portanto os itens seguem
parciais. Arquivos do usuário preservados.

## 21/09/2026 — diagnóstico visual ao adicionar restrição incompatível

SK-04: comandos horizontal/vertical/fixar e cotas X/Y avaliam a nova relação antes
de aplicar. Em incompatibilidade, painel lista candidatas apontadas pelo solver,
marca a nova relação e realça linhas/pontos da geometria original. A lista é
explicitamente não mínima. Acesso à revisão de relações existentes permite
remoção deliberada; a tentativa rejeitada não é reaplicada automaticamente.

Quatro testes funcionais passaram: conflictingSketchConstraintIsExplained,
reviewRedundantSketchConstraints, sketchConstraintsSelectionAndPersistence e
namedSketchDimensionFromSelection (seis resultados Qt com setup/cleanup).
Verificados cancelamento, seleção restaurada, documento inalterado na falha,
abertura da revisão, remoção reversível e persistência. Captura do painel em
build/constraint-conflict.png inspecionada. MecaCAD recompilado sem empacotar.

Limites: conflitos de edição direta de fórmulas, outros caminhos de reconstrução
e erros de contorno ainda usam diagnóstico anterior. Não é conclusão de SK-04
ou dos 38 itens. Nenhum arquivo do usuário ou distribuição existente alterado.

## 21/09/2026 — revisão visual das restrições existentes

SK-04: substituída a lista simples de remoção por um painel de revisão com graus
de liberdade, redundâncias indicadas e realce da linha/pontos correspondentes.
Cotas X/Y mostram valores resolvidos. Consulta e cancelamento restauram a seleção
anterior sem mutação; remoção continua transacional e reversível. Identificadores
internos permanecem no tooltip, sem dominar a lista.

Testes reviewRedundantSketchConstraints, sketchConstraintsSelectionAndPersistence
e namedSketchDimensionFromSelection passaram. Inspeção da captura encontrou o
rótulo incorreto Aresta 0 para realce temporário; corrigido para Geometria realçada.
Documentação em CONSTRAINT-REVIEW.md. Build reutilizado; nenhum pacote novo.

Ainda não fecha SK-04: conflitos em tentativas rejeitadas pelo solver precisam de
fluxo próprio de reparo visual. Este painel trata relações existentes, não amplia
o solver para curvas e não remove redundâncias automaticamente.

## 21/09/2026 — escolha de pivô diretamente no STL

UX-05/UX-01: o modo de escolha de pivô aceita vértices visíveis da triangulação
STL, com a mesma tolerância em pixels usada nos vértices CAD e verificação de
oclusão por superfícies. Disponível somente no comando de pivô: seleção normal
não passa a expor índices de malha como topologia CAD editável. Confirmação,
cancelamento e retorno à prévia desativam a seleção temporária.

O pivô continua armazenado em coordenadas px/py/pz, sem vínculo associativo com
um nó da malha. Isso resolve a limitação de escolher o ponto com mouse, mas não
implementa edição paramétrica da triangulação nem índices topológicos estáveis.
Não há qualificação de desempenho para malhas grandes nesta revisão.

Evidências: meshPivotPickingVisibility sem janela (três zooms, oclusão e seleção
limitada ao comando); pickRotationPivotVertex ampliado para CAD/STL, clique,
operação sem alteração e cancelamento durante a escolha; regressões de pivô
numérico, planos de translação e anel de rotação. A primeira execução do teste
sem janela expôs falta de refresh na montagem da fixture; corrigida antes da
validação. Não houve nova distribuição nem alteração no arquivo do usuário.

## 21/09/2026 — alças de translação nos planos globais

UX-05: manipulador Move / Copy agora oferece quadrados XY/XZ/YZ, com hover,
tooltip e realce do plano ativo. Arraste faz interseção com o plano escolhido,
mantendo a coordenada normal, em vez de apenas projetar o deslocamento da tela.
Encaixe não arredonda a coordenada bloqueada. Alças quase de lado são ocultadas
e rejeitadas para evitar deslocamentos exagerados. Setas por eixo e centro livre
continuam disponíveis; vale também para STL.

Validação: planarMoveHandles percorreu os três planos em CAD/STL com mouse,
prévia e undo; planarDragPreservesNormalCoordinate verificou encaixe com valores
não arredondados e plano de lado. Regressões freeMoveLivePreview, rotationRing,
customRotationPivot e pickRotationPivotVertex passaram: seis casos funcionais,
oito resultados Qt incluindo setup/cleanup, zero falhas. Captura build/planar-move.png
inspecionada visualmente. MecaCAD e ui_tests recompilados em build existente.

Limites: planos globais, não planos locais arbitrários; vértices STL ainda não
são alvos de pivô. UX-05 permanece parcial até o aceite consolidado, sem aumentar
contagem de itens concluídos. dist e desenho do usuário preservados.

## 21/09/2026 — centro de giro configurável

UX-05: Move / Copy permite ativar um pivô personalizado por coordenadas globais
ou escolher um vértice CAD com o mouse. Durante a escolha aparece o corpo original;
depois retorna a prévia com o anel no pivô escolhido. Desativar o pivô personalizado
retorna ao centro da caixa envolvente. Escolher um pivô sem mover/girar não cria
etapa no histórico. Cancelar restaura seleção e filtro anteriores.

O pivô numérico funciona em CAD e STL; a seleção de vértices de malha STL ainda
não é suportada. É possível usar um vértice CAD como referência para mover STL.
Coordenadas seguem a precisão de 0,001 mm dos controles existentes. Não há novos
campos no formato nativo: px/py/pz já eram parâmetros das transformações.

Testes direcionados: customRotationPivot (CAD/STL, prévia, cancelar, undo/redo,
persistência), pickRotationPivotVertex (clique real e operação sem alteração),
rotationRing, freeMoveLivePreview e editSelectedElementsAndBodies.
Build reutilizado, sem pacote novo e sem alterar dist. UX-05 continua parcial:
isto não equivale ao aceite integral dos manipuladores nem dos 38 itens.

## 21/09/2026 — interseções curvas no encaixe inteligente

SK-11: atração a cruzamentos entre segmentos/círculos/arcos. Tangências produzem
um alvo; círculos coincidentes não inventam interseção. Arcos são filtrados pelo
trecho real, incluindo orientação inversa e arco maior que meia volta. Busca de
candidatos circulares limitada à vizinhança do cursor; histerese continua até
15 pixels. Tooltip distingue encaixe temporário de restrição permanente.

Evidências: snap_tests passou com quatro casos matemáticos e setup/cleanup;
UI arcIntersectionDoesNotExtendArc, curvedIntersectionsSnapOnSketchPlanes e
smartSketchSnapping passaram. Verificadas coordenadas nos planos XY/XZ/YZ,
dois zooms, desativação e ausência de alterações no documento. Comando
`sh scripts/check.sh snapping` disponível sem janelas; registrado no CTest.

MecaCAD recompilado somente em build/. Distribuição e desenhos preservados.
Este avanço não fecha os 38 itens nem declara suporte a splines/elipses.

## 21/09/2026 — cotas X/Y com fórmulas no canvas

PAR-04/SK-03: cotas de distância X/Y do solver aceitam parâmetros nomeados.
No menu Constraints, uma linha ou dois vértices geram uma cota com fórmula em
uma transação. No canvas, clicar no valor permite editar número ou expressão;
falha mantém o editor aberto com erro, Esc cancela. Remover uma cota também
remove seu vínculo, preservando os parâmetros do projeto e o undo.

Core expressionsDriveConstrainedDimensions cobre reconstrução de sólido,
graus de liberdade, unidades incompatíveis, colapso do perfil, desvinculação,
remoção/undo e persistência. Passou junto dos testes de sketches restritos.
UI namedSketchDimensionFromSelection, inlineDimensions, namedParametersEditing
e sketchConstraintsSelectionAndPersistence passaram, incluindo cancelamento,
edição de fórmula diretamente no canvas e rejeição de zero sem alterar projeto.

Limite explícito: cotas angulares/radiais e entidades curvas no solver continuam
pendentes; não é fechamento integral de SK-03. Sem novo pacote de aplicativo.

## 21/09/2026 — seleção dinâmica de filete/chanfro

GEO-03/04 e PAR-06: painel permite selecionar arestas no corpo original, com
substituição por clique e adição/remoção com Shift, alternando para a prévia.
Filete existente agora reabre suas medidas e arestas no mesmo recurso, assim
como chanfro. Aceitar sem alterar preserva os parâmetros; cancelar restaura a
seleção anterior. Seleção vazia explícita não se transforma em todas as arestas.

UI dynamicOperationEdgesAndFilletReedit, chamferPreviewEditAndCancel e
selectedEdgeFilletAndMeasurement passaram. O novo teste detectou a limpeza
indevida da multisseleção ao atualizar o viewport; corrigida e retestada.
Nenhuma distribuição criada. Referências topológicas persistentes seguem pendentes.

## 21/09/2026 — supressão e reativação de etapas (PAR-05)

- Supressão explícita e propagação pelo grafo sem apagar histórico. Corpos de
  entrada voltam a aparecer; ramos independentes permanecem ativos.
- Reativação transacional: uma falha não substitui o último documento válido.
  Salvar/reabrir, undo/redo e parâmetros nomeados preservam o estado.
- A interface diferencia suprimida de inativa por dependência; seleção no canvas
  e exportação não incluem geometria inativa. Comando no menu Edit e contextos.
- `.mcad` v4 registra somente a intenção de supressão, não o cache derivado.
  v1/v2/v3 continuam aceitos. Não é uma nova distribuição do aplicativo.
- Suíte core completa: 38 resultados (36 testes e setup/cleanup), zero falhas.
  Suíte recovery: 9 resultados, zero falhas, incluindo recuperação v4.
  UI suppressionFromHistory e historyDependenciesAndReorder: zero falhas.
  Captura build/suppression-history.png inspecionada: etapa identificada no
  Browser/histórico e corpo anterior visível. Fixture v4-suppressed.mcad passou
  no teste conjunto de leitura/regravação das quatro versões nativas.

O aceite funcional de PAR-05 está verificado em desenvolvimento. A linha não é
declarada entregue: falta a porta global de verificação do pacote consolidado,
cuja geração/substituição permanece restringida. Não há ampliação dos 38 itens.

## 21/09/2026 — fórmulas em sketches/ângulos e reedição de extrusão

Escopo congelado nos 38 parciais. Nenhum pacote gerado ou distribuição alterada.

- PAR-04: vínculos de comprimento ampliados para sketches básicos, posições,
  furos e pivôs; ângulos de revolução, movimento e chanfro aceitam deg/rad.
  Tipos dimensionais incompatíveis são recusados sem alterar o documento.
- REL-01/02 e GEO-01: reabrir e confirmar extrusão vinculada preserva precisão,
  fórmula, campos opcionais ausentes e estado limpo. Manipulador e campo numérico
  não sobrescrevem a fórmula. Prévia e confirmação usam os mesmos parâmetros.
- Testes direcionados: core expressionsDriveSketchAndAngles,
  namedParametersDriveGeometry e expressionsOnSupportedFeatures passaram;
  UI expressionBoundExtrusionEditIsNoOp, namedParametersEditing,
  extrusionStartsAtZero, repeatExtrudeEditsExistingBody e extrudeCutPreview passaram.
  Foram abertas somente as janelas necessárias para esses cenários.
- O novo teste de UI detectou arredondamento/alteração de campos opcionais no
  caminho de reedição. Corrigido e repetido com sucesso; não foi removido do aceite.

Ainda não há vínculo de fórmulas com cotas do solver nem fechamento dos 38.
Os testes acima são evidência destas mudanças, não equivalência com Fusion.

Complemento PAR-04: tabela mostra resultados somente leitura e erros de fórmula
enquanto se edita; ciclos desabilitam OK, corrigir reabilita, cancelar preserva
o documento. UI parameterValuesAndErrorsBeforeApply e namedParametersEditing
passaram. Nenhuma nova distribuição foi criada.

## Política de disco — 21/09/2026

Por solicitação do usuário, não gerar novos pacotes ou arquivos versionados do
aplicativo. Desenvolvimento e testes reutilizam build/. A distribuição atual
dist/MecaCAD-0.2.25.app, os desenhos e o histórico Git ficam preservados.
scripts/package-macos.sh recusa execução antes de criar arquivos; a retomada
automática recebeu a mesma restrição. Empacotamento futuro requer nova autorização.

Limpeza autorizada: 14 diretórios temporários build/package-0.2.*, inspecionados
individualmente e contendo somente bundles gerados e metadados do Finder,
aproximadamente 1,3 GiB. Remoção definitiva, não enviada à Lixeira.

## 21/09/2026 — integridade de documento e testes direcionados

Responsável: Codex. Base publicada: 0.2.25. Alterações somente em desenvolvimento;
nenhum aplicativo intermediário foi empacotado e nenhuma janela foi aberta.

### Itens abordados

- REL-01/02: edição sem mudança não cria histórico, não marca documento limpo
  como alterado e não apaga redo. Exclusão de ID inexistente é recusada.
- REL-04: validação estrutural do documento antes de reconstruir geometria;
  tipos de campos, versão exata, unidades, IDs duplicados, modos e planos inválidos
  são recusados. Campos desconhecidos de documento/operação são recusados para
  evitar sua perda silenciosa ao salvar. Parâmetros extras são preservados.
- Base de PAR-02: referências ausentes, futuras ou circulares são recusadas antes
  do cálculo. Isso valida a ordem atual; ainda não é um grafo incremental/reordenável.
- REL-06/QA-01: executor de testes direcionados sem UI por padrão, separação
  headless/interactive e porta de testes completos antes do empacotamento.

### Evidências

Os grupos direcionados executados passaram, sem falhas:

1. rejectedDocumentsPreserveSession, noOpCommandsPreserveHistory,
   failedSaveAndParsePreserveSession, legacyV1FixtureRoundTrip,
   invalidLoadIsAtomic, atomicBatchEdit, rollbackAndDependencies,
   associativeFaceSketchCut.
2. invalidOperationModesAreAtomic, rejectedDocumentsPreserveSession,
   legacyV1FixtureRoundTrip, parametricRoundTrip, operations, revolveAndArc.

Os testes verificam geometria/documento, caminho do arquivo, estado dirty,
undo/redo após falhas e preservação do arquivo original quando uma gravação falha.
A fixture v1-basic.mcad é sintética, não um arquivo real recuperado de cada release.

### Como executar sem interromper o trabalho na tela

- sh scripts/check.sh core: grupo de integridade sem GUI.
- sh scripts/check.sh core nomeDoTeste: somente os casos informados.
- sh scripts/check.sh ui nomeDoTeste: testes visuais explicitamente escolhidos;
  podem abrir janelas e devem ser avisados antes.
- sh scripts/check.sh release: build e suítes completas; reservado à validação
  de entrega ou mudança estrutural que justifique sua execução.

O empacotador chama release antes de produzir um novo pacote. Não foi executado
nesta etapa; a integração da porta foi inspecionada e sua sintaxe verificada.
Não há servidor de CI configurado ainda.

### Limites e sequência seguinte

REL-01/02/04 continuam parciais: falta auditar todos os comandos, ampliar fixtures
históricas e testar gravações interrompidas por falha de processo/disco.
REL-06 passou de pendente para parcial, não concluído. A suíte completa e a UI
não foram executadas nesta etapa, por escolha de testes proporcionais à mudança.

## 21/09/2026 — recuperação isolada e estado salvo

Alterações internas, sem novo pacote de entrega.

- REL-03: diário atômico por sessão, com trava de proprietário; sessões ativas
  não aparecem como recuperáveis e uma janela não limpa a recuperação de outra.
- Recuperação abre uma cópia não salva, exige novo caminho ao salvar e preserva
  o arquivo original. A cópia antiga só é retirada após gravar a nova recuperação.
- Menu File → Recover unsaved project permite escolher uma sessão interrompida;
  cancelar não altera o documento ou o diário. Falhas de leitura são preservadas.
- REL-02: undo/redo compara o documento com o último estado salvo. Voltar a esse
  estado remove o indicador de alteração e a recuperação obsoleta. Uma cópia
  recuperada continua não salva mesmo após editar e desfazer.
- Descartar alterações e depois cancelar a escolha de outro arquivo não apaga
  antecipadamente a recuperação do documento ainda aberto.

### Evidências direcionadas

- Core: savedStateTracksUndoRedo, noOpCommandsPreserveHistory,
  rejectedDocumentsPreserveSession, failedSaveAndParsePreserveSession passaram.
- Recovery: failedWriteKeepsPreviousCopy, dirtyDestinationIsNotReplaced,
  sessionsStayIsolated, corruptRecoveryIsPreserved,
  forcedTerminationRestoresWithoutOverwritingOriginal passaram sem janelas.
- O encerramento forçado é real, em processo de teste próprio; verifica também
  que os bytes do arquivo original não mudam.
- UI: isolatedRecoveryDialog passou no fluxo cancelar → recuperar → persistir
  em novo caminho → fechar. Não automatiza o diálogo nativo Save As.

### Limites restantes

REL-03 permanece parcial. O intervalo é de 30 segundos: alterações posteriores
ao último diário podem ser perdidas. Falta testar queda durante a gravação,
disco cheio e recuperação de projetos grandes. Registros com envelope ilegível
ficam preservados em disco, mas não aparecem na lista. O recovery.mcad legado
não é apagado nem migrado automaticamente; pode ser aberto manualmente.

Executar apenas recuperação: sh scripts/check.sh recovery.
Não houve suíte completa nem publicação de aplicativo nesta etapa.
O restante do backlog continua pendente/parcial conforme BACKLOG.md.

## 21/09/2026 — grafo explícito e reordenação transacional

Responsável: Codex. Dependências: validação de documentos e histórico existentes.
Risco principal: regressão de reconstrução, referências e undo/redo. Sem pacote
intermediário; alterações internas sobre a base 0.2.25.

- PAR-02/PROD-03: módulo de dependências independente da geometria/UI; entradas
  source/target/tool/support, diagnóstico distinto para ausência, ciclo e ordem
  futura; enumeração de dependentes diretos/indiretos e ordenação topológica.
- REL-01/PAR-03: reconstrução em modelo candidato; falhar não limpa as formas do
  documento vigente nem exige reconstruir o estado antigo para recuperá-lo.
  Erros de parâmetros identificam nome e ID da etapa. Exclusão recusada lista
  as operações dependentes.
- PAR-05: mover uma etapa antes/depois no histórico, com validação de referências,
  undo/redo único e persistência. Disponível em Edit e menus contextuais do Browser
  e da timeline. Reordenação recusada durante desenho de sketch.
- PAR-03: relatório de entradas e dependentes da etapa na interface, sem mutação.

Evidências do núcleo: dependencyGraphValidation, failedRebuildPreservesGeometry
e historyReorderingIsTransactional. Verificam grafo em diamante, referências
duplicadas, ausentes, ciclos, ordem futura, preservação das próprias formas CAD
em falhas, persistência e histórico. Suíte core passou após a mudança estrutural;
reordenação e regressões afetadas passaram após inclusão do comando.
Suíte recovery passou também.
Interface: historyDependenciesAndReorder, isolatedRecoveryDialog,
extrudeCutPreview e faceSelectionAndSketch passaram com eventos Qt e janelas
temporárias, sem abrir documentos do usuário. Build de MecaCAD passou.

Limites: PAR-02 e PAR-05 permanecem parciais. A reconstrução ainda recalcula todas
as etapas; não há cache incremental, supressão/reativação ou arraste da timeline.
O relatório não substitui editor de reparo de referências. As referências de
faces/arestas ainda dependem da topologia descrita nas limitações anteriores.

## 21/09/2026 — domínio de sketch e solver afim interno

Responsável: Codex. Itens SK-01/02/04 e PROD-03/04. Dependência para integração:
migração do documento nativo, validade dos contornos e comandos transacionais.
Risco principal: confundir convergência algébrica com geometria CAD utilizável.

- Novo módulo mecasketch, independente de Widgets e Open CASCADE, com IDs de
  pontos/linhas/restrições e serialização interna estrita.
- Solver geral de equações afins para coincidência, horizontal/vertical, fixação
  e diferenças assinadas X/Y; menor deslocamento da geometria inicial, graus de
  liberdade, redundâncias e contribuintes candidatos de conflito.
- Prévia pura e aplicação em cópia; falha não altera o sistema original.
- Revisão inicial das alternativas SolveSpace/PlaneGCS e limites da decisão em
  [SKETCH-SOLVER.md](SKETCH-SOLVER.md). Nenhuma nova biblioteca de terceiros foi
  incorporada. A auditoria completa de dependências continua pendente.

Validação: 8 casos funcionais de sketch_system_tests passaram, mais inicialização
e encerramento (10 resultados, zero falhas). Incluem cadeia de 128 pontos,
variação de escala/ordem, persistência JSON em memória, entradas inválidas,
conflitos, graus de liberdade e IDs estáveis. Executar sh scripts/check.sh sketch.
Nenhuma janela aberta, pacote gerado ou arquivo do usuário alterado. A compilação
reutiliza build/ e o empacotamento continua bloqueado.

SK-02 e SK-04 passam a parciais, não concluídos. Este módulo NÃO está ligado à
interface ou ao formato .mcad: o aplicativo distribuído não ganhou ferramentas
de restrição nesta etapa. Falta integrar edição/arraste e migração, validar
contornos após resolver e oferecer diagnóstico visual. Curvas, tangência e
ângulos exigem solver não linear; não são suportados por este núcleo afim.

## 21/09/2026 — integração inicial das restrições e escopo fixo de 35 itens

O usuário solicitou fechar os 35 itens parciais, não todo o produto nesta entrega.
A lista foi congelada no início do BACKLOG.md e a retomada automática ajustada.
Dependências indispensáveis entram no trabalho, mas novos itens parciais não
aumentam automaticamente essa lista. Nenhum item foi marcado concluído nesta etapa.

- SK-01/02/04 e REL-04: constraintSystem passa a ser persistido em .mcad v2;
  projetos sem restrições continuam v1. v1 contendo restrições é recusado.
  A versão v2 é do formato de documento, não um novo pacote do aplicativo.
- Conversão de retângulo/polilinha em contorno restrito com IDs preservados;
  retângulo mantém relações horizontal/vertical, mas não ganha cotas fixas implícitas.
- Solver integrado à reconstrução transacional. Pontos resolvidos alimentam
  geometria CAD e extrusões dependentes. Reabertura e rebuild são idempotentes.
- Comandos Horizontal, Vertical e Fixar ponto por seleção no canvas, com ícones,
  indicação de graus de liberdade e remoção individual de restrições.
- Prévia/resolução inválida não altera o documento; contornos colapsados,
  cruzados, sobrepostos ou sem área são recusados.

Validações já executadas: suíte core (29 resultados incluindo setup/cleanup),
sketch_system (10) e UI sketchConstraintsSelectionAndPersistence (3), sem falhas.
O teste de UI clica na linha e no botão Horizontal e verifica remover/cancelar,
undo/redo e persistência. Captura inspecionada em build/sketch-constraints.png.
Na verificação adicional passaram constrainedSketchDocumentRoundTrip,
constrainedSketchRejectsInvalidContours, legacyV1FixtureRoundTrip,
savedStateTracksUndoRedo e os cinco casos de recovery, além de setup/cleanup.

Limites: somente contorno simples de segmentos. Coincidência entre entidades
independentes, curvas, cotas gerais, conflito com realce no canvas, diagnóstico
completo e arraste com prioridade do cursor ainda faltam. Excluir subelementos
paramétricos é recusado para não quebrar vínculos. Conversão de retângulo muda a
representação para polilinha; cotas retangulares antigas não são mostradas nesse
perfil, pendência de SK-03. Não há nova distribuição; dist continua preservado.

Verificação final da revisão: reconstrução limpa dos quatro executáveis de teste,
seguida de core (29), sketch_system (10), recovery (7) e do teste de UI acima (3):
49 resultados incluindo setup/cleanup, zero falhas. Inclui rejeição de contorno
cruzado/sem área e gesto em sketch totalmente fixo sem criar alteração ou undo.
A pasta build/ desapareceu durante uma compilação intermediária, sem comando de
remoção desta tarefa; foi recriada somente para desenvolvimento/testes. Nenhum
arquivo em dist foi substituído. Os 35 itens continuam em execução, não aceitos.

## 21/09/2026 — mais 30 itens e parâmetros nomeados

Pedido: acrescentar mais 30 itens. BACKLOG.md registra 30 IDs distintos dos 35
originais, totalizando 65 no escopo. Seleção prioriza CAD diário e dependências;
não implica 30 implementações concluídas. A continuação automática existente foi
atualizada, preservando frequência, tarefa e restrições de disco/privacidade.

Responsável: Codex. PAR-04 passa a parcial. Dependências: documento transacional,
unidades, reconstrução, histórico; riscos: ciclos, incompatibilidade dimensional,
perda de fórmulas ao editar numericamente e compatibilidade com arquivos anteriores.

- Avaliador próprio sem scripts ou execução de código, limitado a aritmética,
  unidades e referências nomeadas; isolamento de Qt Widgets e kernel geométrico.
- Resolução de dependências com detecção de ciclos, valores não finitos e limites.
- Tabela editável de parâmetros e vínculo de medidas em blocos, cilindros, esferas,
  extrusões e filetes. Mudança válida reconstrói os dependentes; falha é atômica.
- Campos vinculados são somente leitura nas propriedades. Edição genérica não
  descarta vínculos silenciosamente; remoção explícita conserva a medida resolvida.
- Documento v3 apenas quando necessário; v1/v2 continuam legíveis. Salvar,
  reconstruir e undo/redo preservam definições e vínculos.

Validação: suíte core completa (30 resultados incluindo setup/cleanup), expressões
(7), recovery (7) e UI direcionada parâmetros + restrições (4), sem falhas na
revisão verificada. Teste de UI verifica vincular, redimensionar, cancelar, undo/redo
e salvar/reabrir. Captura build/parameter-editor.png inspecionada, sem cortes ou
controles encobertos. A regressão detectou uma leitura Qt que inseria um campo
nulo em sketches legados; corrigida e testes v1/v2 repetidos com sucesso.

Limitações em [PARAMETERS.md](PARAMETERS.md): vínculos ainda não abrangem cotas de
sketch, ângulos ou todos os campos; não há renomeação associativa/autocomplete.
Nenhum item inteiro foi encerrado. Sem pacote, alteração em dist ou projeto do
usuário; uso de build/ único. A verificação adicional expressionsOnSupportedFeatures
e namedParametersDriveGeometry passou (4 resultados com setup/cleanup), cobrindo
cada tipo de recurso compatível e mistura de restrições v2 com parâmetros v3.

## 21/09/2026 — segundo lote de 30, chanfro e corpus sintético

Novo pedido de mais 30 itens: escopo agora de 95 IDs distintos dos 101 do backlog,
contagem conferida automaticamente, sem IDs inexistentes ou repetidos. A lista do
segundo lote e os seis itens fora do escopo estão no início de BACKLOG.md. Retomada
automática atualizada, preservando a cadência e todas as restrições anteriores.
Isso amplia o escopo, não a contagem de itens aceitos.

Responsável: Codex. Trabalho realizado em GEO-04 e QA-05, ambos parciais. Dependências
de implementação: kernel OCCT já instalado, seleção por aresta, prévia transacional,
persistência e histórico. Risco principal: troca silenciosa de referência topológica;
guarda por contagem é apenas proteção parcial, não substitui PAR-01.

- Chanfro em três modos: igual distância, duas distâncias e distância/ângulo;
  troca de face de referência, seleção por arestas, validação de distâncias/ângulo.
- Prévia sem alteração do documento; falha impede confirmação. Reabrir o recurso
  pelo comando edita a mesma etapa, sem duplicar o corpo. Undo/redo e arquivo nativo.
- Distâncias vinculáveis a fórmulas, protegidas contra arredondamento/sobrescrita
  no painel. Implementação geométrica separada em src/chamfer.cpp.
- Corpus sintético de suporte, caixa de sensor e espaçador; sólidos válidos,
  volume analítico, reconstrução, salvar/reabrir e STEP. Sem copiar projetos privados.

Validação: suíte core completa (33 resultados incluindo setup/cleanup), UI
chamferPreviewEditAndCancel + selectedEdgeFilletAndMeasurement (4), corpus (5).
Todos passaram. O teste de UI percorre seleção com mouse, prévia, valor impossível,
edição sem duplicação, cancelar, undo/redo e persistência. A captura
build/chamfer-preview.png foi inspecionada: prévia geométrica e controles legíveis.
Documentação e limites em CHAMFER.md e QA-CORPUS.md; troca STEP usa o próprio OCCT,
não é validação por aplicativo independente.

Não foram concluídos 30 itens nesta execução. Nenhum item inteiro foi marcado
aceito. Nenhum pacote ou nova versão .app foi produzido; dist e arquivos do usuário
foram preservados. Próximos trabalhos continuam por dependência, não para inflar
contagem com botões ou núcleos isolados.

## 21/09/2026 — foco nos 38 parciais: recuperação e compatibilidade

O pedido mais recente substitui a ampliação de escopo: concluir os 38 parciais,
sem adicionar itens. BACKLOG.md e retomada automática foram ajustados. Pergunta
de autorização para atualizar o aplicativo existente sem cópias foi enviada; até
resposta explícita, dist continua intocado. Não reduzir aceites para mudar contagem.

REL-03: cópias ilegíveis agora são reportadas sem apagá-las; falha de recuperação
fica visível num indicador próprio, não é apagada por um refresh normal. O timer
real da janela foi testado numa instância filha encerrada à força; arquivo original
comparado byte a byte, dados v3 recuperados pela interface como cópia não salva.
Recuperação de documentos v1/v2/v3 e isolamento de sessões verificados.

REL-04: fixtures permanentes v1/v2/v3, volume esperado, reconstrução idempotente,
salvar/reabrir e rejeição atômica de versão futura. A versão inicial publicada
(895d7bd) já usava documento v1; v2/v3 são extensões recentes de desenvolvimento.
PAR-02: cadeia de 2.000 nós, ciclo no limite e falha em operação dependente sem
alterar documentos, shapes ou undo/redo. Mantido aceite funcional original.

Testes desta revisão: recovery (9 resultados), UI direcionada de recuperação e
histórico (6), core fixtures/limite de grafo (4), incluindo setup/cleanup; zero
falhas. Janelas usadas somente nos testes necessários; nenhuma versão empacotada.
O fechamento da entrega consolidada ainda exige os outros aceites e publicação
autorizada; não é declaração de conclusão dos 38.
