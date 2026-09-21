# Execução do backlog

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
