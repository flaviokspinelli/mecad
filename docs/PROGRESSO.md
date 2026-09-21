# Execução do backlog

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
