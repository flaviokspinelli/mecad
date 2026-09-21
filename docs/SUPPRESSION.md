# Suprimir e reativar etapas

Implementação de desenvolvimento; a distribuição 0.2.25 permanece intacta.

Selecione uma etapa no histórico ou Browser e use **Edit → Suprimir / reativar
etapa**, também disponível no menu de contexto. A etapa fica identificada como
suprimida. Seus dependentes ficam inativos por dependência, sem perder parâmetros
ou identidade. Reative a origem para voltar a calculá-los.

Isso não é ocultar: uma etapa suprimida deixa de consumir seus corpos de entrada,
permitindo visualizar o resultado anterior. Também não é apagar: o histórico
permanece completo. Etapas independentes não são desativadas. Uma etapa já
suprimida explicitamente continua assim mesmo após reativar outro ancestral.

Supressão e reativação são transações com undo/redo. Se a reconstrução falhar ao
reativar, o documento anterior permanece válido e suprimido. Entradas ausentes,
ciclos e reordenações impossíveis continuam sendo recusados, inclusive entre
etapas suprimidas. Geometria inativa não pode ser selecionada no canvas ou
exportada. Suprimir uma etapa não apaga suas fórmulas.

## Arquivo nativo

Documentos contendo supressão usam `.mcad` v4, JSON simples, sem ZIP.
O campo opcional booleano `suppressed` pertence à etapa; o estado derivado
`inactive` não é salvo. Ao abrir, ele é reconstruído pelo grafo de dependências.
Arquivos v1/v2/v3 continuam aceitos. Leitores anteriores recusam v4 em vez de
abrir silenciosamente uma geometria diferente. Reativar todas as etapas permite
salvar novamente na versão mínima exigida pelo conteúdo.

## Verificação

- Core: suppressionPreservesHistoryAndDependencies, failedReactivationIsAtomic.
- Interface: suppressionFromHistory e historyDependenciesAndReorder.
- Recuperação: everyNativeVersionRestores cobre também v4.

Sem remapeamento automático de referências quebradas. Suprimir não ignora nem
corrige a dependência: ela precisa existir e respeitar a ordem do histórico.
