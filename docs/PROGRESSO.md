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

Próxima frente: REL-03, isolamento das recuperações por documento/sessão e teste
de falha forçada, sem sobrescrever projeto ou recuperação de outra janela.
Depois, prosseguir para o modelo de sketch e grafo de dependências do backlog.
