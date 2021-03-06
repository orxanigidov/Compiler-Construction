#include "Statement.h"
#include "AssignmentStatement.h"
#include "WhileStatement.h"
#include "IfStatement.h"
#include "ProcedureCallStatement.h"

AssignmentStatement::AssignmentStatement(std::shared_ptr<const Expression> _expression) : expression(_expression) { }

ProcedureCallStatement::ProcedureCallStatement() {}

WhileStatement::WhileStatement(std::shared_ptr<const Expression> _expression) : expression(_expression) {}

IfStatement::IfStatement(std::shared_ptr<const Expression> _expression): expression(_expression) { }

ElseIf::ElseIf(std::shared_ptr<const Expression> _expression): expression(_expression) {}

Else::Else() {}