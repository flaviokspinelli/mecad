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
