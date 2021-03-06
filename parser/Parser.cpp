//
// Created by Michael Grossniklaus on 11/20/18.
//

#include <iostream>
#include "Parser.h"
#include "IdentToken.h"
#include "StringToken.h"
#include "NumberToken.h"

#include "StringFactor.h"
#include "NumberFactor.h"
#include "BooleanFactor.h"
#include "ExpressionFactor.h"
#include "NotFactor.h"
#include "VariableFactor.h"
#include "RecordSelector.h"

Parser::Parser(Scanner* scanner, Logger* logger) :
	scanner_(scanner), logger_(logger) {
	symbolTable_ = SymbolTable();
}

Parser::~Parser() = default;

const std::unique_ptr<const Module> Parser::parse() {
	return module();
}

const std::string Parser::ident() {
	if (token_->getType() == TokenType::const_ident) {
		return static_cast<const IdentToken*>(token_.get())->getValue();
	}
	return "";
}

std::unique_ptr<const Module> Parser::module()
{
	// "MODULE" ident ";" declarations ["BEGIN" StatementSequence] "END" ident "."
	token_ = scanner_->nextToken();
	if (token_->getType() == TokenType::kw_module)
	{
		token_ = scanner_->nextToken();
		std::string identifier = ident();
		if (!identifier.empty())
		{
			token_ = scanner_->nextToken();
			if (token_->getType() == TokenType::semicolon)
			{
				token_ = scanner_->nextToken();
				auto _module = std::make_unique<Module>(identifier);
				logger_->info("", identifier + " module is created.");
				for (auto& declaration : declarations()) {
					bool isDeclarationOk = true;
					for (auto moduleDeclaration : _module->declarations)
						if (declaration->identifier == moduleDeclaration->identifier) {
							logger_->error(token_->getPosition(), "- SEMANTIC ERROR; Identifier \"" + declaration->identifier + "\" has been used");
							isDeclarationOk = false;
						}
					if (isDeclarationOk)
					{
						_module->declarations.emplace_back(declaration);
						logger_->info("", "A " + declaration->identifier + " declaration is added to " + identifier + " Module.");
					}
				}

				if (token_->getType() == TokenType::kw_begin)
				{
					token_ = scanner_->nextToken();
					_module->statements = statement_sequence();
				}
				if (token_->getType() == TokenType::kw_end)
				{
					token_ = scanner_->nextToken();
					identifier = ident();
					if (!identifier.empty())
					{
						if (_module->identifier == identifier) {
							token_ = scanner_->nextToken();
							if (token_->getType() == TokenType::period)
							{
								token_ = scanner_->nextToken();
								return _module;
							}
							else
							{
								logger_->error(token_->getPosition(), "- SYNTAX ERROR; \".\" is missing.");
							}
						}
						else {
							logger_->error(token_->getPosition(), "- SEMANTIC ERROR; Module name is not same at the end.");
						}
					}
					else {
						logger_->error(token_->getPosition(), "- SEMANTIC ERROR; Module name is missing.");
					}
				}
				else {
					logger_->error(token_->getPosition(), "- SYNTAX ERROR; \"END\" keyword is missing");
				}
			}
			else {
				logger_->error(token_->getPosition(), "- SYNTAX ERROR; \";\" is missing");
			}
		}
		else {
			logger_->error(token_->getPosition(), "- SYNTAX ERROR; MODULE idetifier is not valid.");
		}
	}
	else {
		logger_->error(token_->getPosition(), "- SYNTAX ERROR; \"MODULE\" keyword is missing.");
	}
	return nullptr;
}

const std::vector<std::shared_ptr<const Variable>> Parser::declarations() {
	// ["CONST" {ident "=" expression ";"}]
	// ["TYPE" {ident "=" type ";"}]
	// ["VAR" {IdentList ":" type ";"}]
	// {ProcedureDeclaration ";"}.
	std::vector<std::shared_ptr<const Variable>> declarationList;
	while (token_->getType() == TokenType::kw_const || token_->getType() == TokenType::kw_type || token_->getType() == TokenType::kw_var || token_->getType() == TokenType::kw_procedure)
	{
		switch (token_->getType())
		{
		case TokenType::kw_const:
			for (auto constantDeclaration : const_declarations()) {
				if (constantDeclaration != nullptr)
				{
					declarationList.emplace_back(constantDeclaration);
				}
			}
			break;
		case TokenType::kw_type:
			for (auto typeDeclaration : type_declarations()) {
				if (typeDeclaration != nullptr)
				{
					declarationList.emplace_back(typeDeclaration);
				}
			}
			break;
		case TokenType::kw_var:
			for (auto varDeclaration : var_declarations()) {
				if (varDeclaration != nullptr)
				{
					declarationList.emplace_back(varDeclaration);
				}
			}
			break;
		case TokenType::kw_procedure:
			auto procedureDeclaration = procedure_declaration();
			if (procedureDeclaration != nullptr)
			{
				declarationList.emplace_back(procedureDeclaration);
			}
			break;
		}
	}
	return declarationList;
}

const std::vector<std::shared_ptr<const ConstVariable>> Parser::const_declarations() {
	// "CONST" {ident "=" expression ";"}
	std::vector<std::shared_ptr<const ConstVariable>> constantDeclarations;
	token_ = scanner_->nextToken();
	std::string identifier = ident();
	if (!identifier.empty())
	{
		bool shouldRepeat = true;
		while (shouldRepeat)
		{
			token_ = scanner_->nextToken();
			if (token_->getType() == TokenType::op_eq)
			{
				token_ = scanner_->nextToken();
				auto _expression = expression();
				if (_expression.get() != nullptr)
				{
					if (token_->getType() == TokenType::semicolon)
					{
						auto constantVariable = std::make_shared<const ConstVariable>(identifier, _expression);
						constantDeclarations.emplace_back(constantVariable);
						symbolTable_.insert(identifier, constantVariable);
						logger_->info("CONST Declaration", "Name: " + identifier);
						token_ = scanner_->nextToken();
						identifier = ident();
						if (identifier.empty())
						{
							shouldRepeat = false;
						}
					}
					else {
						logger_->error(token_->getPosition(), "- SYNTAX ERROR; \";\" is missing.");
						shouldRepeat = false;
					}
				}
				else {
					logger_->error(token_->getPosition(), "- SEMANTIC ERROR; No valid expression.");
					shouldRepeat = false;
				}
			}
			else {
				logger_->error(token_->getPosition(), "- SYNTAX ERROR; \"=\" is missing.");
				shouldRepeat = false;
			}
		}
	}
	else {
		logger_->error(token_->getPosition(), "- SEMANTIC ERROR; Const declaration name is not valid.");
	}
	return constantDeclarations;
}

const std::vector<std::shared_ptr<const TypeVariable>> Parser::type_declarations() {
	// "TYPE" {ident "=" type ";"} -> repetition
	std::vector<std::shared_ptr<const TypeVariable>> typeDeclarations;
	bool shouldProceed = true;
	token_ = scanner_->nextToken();
	std::string identifier = ident();
	if (!identifier.empty())
	{
		while (shouldProceed)
		{
			token_ = scanner_->nextToken();
			if (token_->getType() == TokenType::op_eq)
			{
				token_ = scanner_->nextToken();
				auto _type = type();
				if (_type.get() != nullptr)
				{
					if (token_->getType() == TokenType::semicolon)
					{
						auto typeDeclaration = std::make_shared<const TypeVariable>(identifier, _type);
						typeDeclarations.emplace_back(typeDeclaration);
						logger_->info("TYPE Declaration", "Name: " + identifier);
						symbolTable_.insert(identifier, typeDeclaration);
						token_ = scanner_->nextToken();
						identifier = ident();
						if (identifier.empty())
						{
							shouldProceed = false;
						}
					}
					else {
						shouldProceed = false;
						logger_->error(token_->getPosition(), "- SYNTAX ERROR; \";\ is missing.");
					}
				}
				else {
					logger_->error(token_->getPosition(), "- SEMANTIC ERROR; No valid type");
					shouldProceed = false;
				}
			}
			else {
				shouldProceed = false;
				logger_->error(token_->getPosition(), "- SYNTAX ERROR; \"=\" is missing.");
			}
		}
	}
	else {
		logger_->error(token_->getPosition(), "- SYNTAX ERROR; Type declaration name is not valid.");
	}
	return typeDeclarations;
}

const std::vector<std::shared_ptr<const VarVariable>> Parser::var_declarations() {
	// "VAR" {IdentList ":" type ";"}
	std::vector<std::shared_ptr<const VarVariable>> varDeclarations;
	token_ = scanner_->nextToken();
	bool shouldRepeat = true;
	while (shouldRepeat)
	{
		std::vector<std::string> identifier_list = ident_list();
		if (identifier_list.size() != 0)
		{
			if (token_->getType() == TokenType::colon)
			{
				token_ = scanner_->nextToken();
				auto _type = type();
				if (_type != nullptr)
				{
					if (token_->getType() == TokenType::semicolon)
					{
						for (auto const& identifier : identifier_list) {
							auto varVariable = std::make_shared<const VarVariable>(identifier, _type);
							varDeclarations.emplace_back(varVariable);
							logger_->info("Var Declaration", "Name: " + identifier);
							symbolTable_.insert(identifier, varVariable);
						}
						token_ = scanner_->nextToken();
					}
					else {
						shouldRepeat = false;
						logger_->error(token_->getPosition(), "- SYNTAX ERROR; \";\ is missing");
					}
				}
				else {
					logger_->error(token_->getPosition(), "- SEMANTIC ERROR; No valid type");
					shouldRepeat = false;
				}
			}
			else {
				shouldRepeat = false;
				logger_->error(token_->getPosition(), "- SYNTAX ERROR; \":\" is missing");
			}
		}
		else {
			//logger_->error(token_->getPosition(), "- SEMANTIC ERROR; Identifier names are not valid or specified");
			shouldRepeat = false;
		}
	}
	return varDeclarations;
}

std::shared_ptr<const ProcedureVariable> Parser::procedure_declaration() {
	// ProcedureHeading ";" ProcedureBody
	auto head = procedure_heading();
	if (head != nullptr)
	{
		if (token_->getType() == TokenType::semicolon)
		{
			token_ = scanner_->nextToken();
			auto body = procedure_body(head->identifier);
			if (body != nullptr)
			{
				auto procedure = std::make_shared<ProcedureVariable>(head->identifier);
				procedure->parameters = head->parameters;
				procedure->declarations = body->declarations;
				procedure->statements = body->statements;
				symbolTable_.insert(head->identifier, procedure);
				logger_->info("Procedure Declaration", "Name: " + head->identifier);
				return procedure;
			}
			else {
				logger_->error(token_->getPosition(), "- SYNTAX ERROR; No valid procedure body");
			}
		}
		else {
			logger_->error(token_->getPosition(), "- SYNTAX ERROR; \";\" is missing");
		}
	}
	else {
		logger_->error(token_->getPosition(), "- SYNTAX ERROR; No valid procedure head");
	}
	return nullptr;
}

std::shared_ptr<const Expression> Parser::expression() {
	// SimpleExpression [("=" | "#" | "<" | "<=" | ">" | ">=") SimpleExpression] -> Optional

	auto lhs = simple_expression();
	if (lhs != nullptr)
	{
		if (token_->getType() == TokenType::op_eq || token_->getType() == TokenType::op_neq || token_->getType() == TokenType::op_lt || token_->getType() == TokenType::op_leq || token_->getType() == TokenType::op_gt || token_->getType() == TokenType::op_geq)
		{
			auto operand = token_->getType();
			token_ = scanner_->nextToken();
			auto rhs = simple_expression();
			if (rhs != nullptr)
			{
				if (lhs->type == rhs->type)
				{
					if ((operand == TokenType::op_lt || operand == TokenType::op_leq || operand == TokenType::op_gt || operand == TokenType::op_geq) && lhs->type != PrimitiveType::Number)
					{
						logger_->error(token_->getPosition(), "- SEMANTIC ERROR; \"<\" | \"<=\" | \">\" | \">=\" should be with numbers");
					}
					else {
						return std::make_shared<const Expression>(lhs, operand, rhs);
					}
				}
				else {
					logger_->error(token_->getPosition(), "- SEMANTIC ERROR; lhs and rhs type should be same");
				}
			}
			else {
				logger_->error(token_->getPosition(), "- SYNTAX ERROR; No rhs simple expression after an operand");
			}
		}
		else {
			return std::make_shared<const Expression>(lhs);
		}
	}
	else {
		logger_->error(token_->getPosition(), "- SYNTAX ERROR; No simple expression");
	}
	return nullptr;
}

std::shared_ptr<const SimpleExpression> Parser::simple_expression() {
	// ["+" | "-"] term {("+" | "-" | "OR") term}
	auto operand = TokenType::null;
	if (token_->getType() == TokenType::op_plus || token_->getType() == TokenType::op_minus) {
		operand = token_->getType();
		token_ = scanner_->nextToken();
	}
	auto _term = term();
	if (_term != nullptr)
	{
		if (operand != TokenType::null && _term->type != PrimitiveType::Number)
		{
			logger_->error(token_->getPosition(), "- SEMANTIC ERROR; \"+\" and \"-\" operands can not use without numbers");
			return nullptr;
		}
		auto simpleExpression = std::make_shared<SimpleExpression>();
		simpleExpression->type = _term->type;
		bool shouldRepeat = true;
		while (shouldRepeat)
		{
			if (_term->type == simpleExpression->type)
			{
				auto element = std::make_shared<const SimpleExpressionElement>(operand, _term);
				simpleExpression->elements.emplace_back(element);
				if (token_->getType() == TokenType::op_plus || token_->getType() == TokenType::op_minus || token_->getType() == TokenType::op_or)
				{
					if ((_term->type == PrimitiveType::Number && (token_->getType() == TokenType::op_plus || token_->getType() == TokenType::op_minus)) || (_term->type == PrimitiveType::Boolean && token_->getType() == TokenType::op_or))
					{
						operand = token_->getType();
						token_ = scanner_->nextToken();
						_term.reset();
						_term = term();
						if (_term == nullptr)
						{
							shouldRepeat = false;
						}
					}
					else {
						logger_->error(token_->getPosition(), "- SEMANTIC ERROR; operand and term type can not matched");
						shouldRepeat = false;
					}
				}
				else {
					return simpleExpression;
				}
			}
			else {
				logger_->error(token_->getPosition(), "- SEMANTIC ERROR; Type inconsistency");
				shouldRepeat = false;
			}
		}
	}
	else {
		logger_->error(token_->getPosition(), "- SEMANTIC ERROR; No valid term");
	}
	return nullptr;
}

std::shared_ptr<const Term> Parser::term() {
	// factor {("*" | "DIV" | "MOD" | "&") factor} -> repetition
	auto _factor = factor();
	if (_factor != nullptr)
	{
		auto term = std::make_shared<Term>();
		term->type = _factor->type;
		auto operand = TokenType::null;
		bool shouldRepeat = true;
		while (shouldRepeat)
		{
			if (_factor->type == term->type)
			{
				auto element = std::make_shared<const TermElement>(operand, _factor);
				term->elements.emplace_back(element);
				if (token_->getType() == TokenType::op_times || token_->getType() == TokenType::op_div || token_->getType() == TokenType::op_mod || token_->getType() == TokenType::op_and)
				{
					if ((_factor->type == PrimitiveType::Number && (token_->getType() == TokenType::op_times || token_->getType() == TokenType::op_div || token_->getType() == TokenType::op_mod)) || (_factor->type == PrimitiveType::Boolean && token_->getType() == TokenType::op_and))
					{
						operand = token_->getType();
						token_ = scanner_->nextToken();
						_factor.reset();
						_factor = factor();
						if (_factor == nullptr)
						{
							shouldRepeat = false;
						}
					}
					else {
						logger_->error(token_->getPosition(), "- SEMANTIC ERROR; operand and term type can not matched");
						shouldRepeat = false;
					}
				}
				else {
					return term;
				}
			}
			else {
				logger_->error(token_->getPosition(), "- SEMANTIC ERROR; Type inconsistency");
				shouldRepeat = false;
			}
		}
	}
	else {
		logger_->error(token_->getPosition(), "- SEMANTIC ERROR; No valid factor");
	}
	return nullptr;
}

std::shared_ptr<const Factor> Parser::factor() {
	// ident selector | integer | "(" expression ")" | "~" factor	
	std::string identifier = ident();
	if (!identifier.empty())
	{
		token_ = scanner_->nextToken();
		auto _variable = symbolTable_.lookup(identifier);
		if (_variable != nullptr)
		{
			Variable* _var;
			if (_variable->nodeType_ == NodeType::variable_reference)
			{
				_var = static_cast<VarVariable*>(const_cast<Node*>(_variable.get()));
			}
			else if (_variable->nodeType_ == NodeType::constant_reference) {
				_var = static_cast<ConstVariable*>(const_cast<Node*>(_variable.get()));
			}
			else {
				logger_->error(token_->getPosition(), "- SEMANTIC ERROR; Type variable can not be used as a factor");
				return nullptr;
			}
			auto _selector = selector();
			if (_selector != nullptr)
			{
				if (_variable->nodeType_ == NodeType::variable_reference)
				{
					if (_var->primitiveType == _selector->type)
					{
						_var->selector = _selector;
					}
					else {
						logger_->error(token_->getPosition(), "- SEMANTIC ERROR; Selector type is not convenient with the variable type");
						return nullptr;
					}
				}
				else {
					logger_->error(token_->getPosition(), "- SEMANTIC ERROR; Invalid selector");
					return nullptr;
				}
			}
			return std::make_shared<const VariableFactor>(std::shared_ptr<Variable>(_var));
		}
		else {
			logger_->error(token_->getPosition(), "- SEMANTIC ERROR; No variable with \"" + identifier + "\"");
		}
	}
	else {
		if (token_->getType() == TokenType::const_string)
		{
			const std::string str = static_cast<const StringToken*>(token_.get())->getValue();
			token_ = scanner_->nextToken();
			return std::make_shared<const StringFactor>(str);
		}
		else if (token_->getType() == TokenType::const_number) {
			const int number = static_cast<const NumberToken*>(token_.get())->getValue();
			token_ = scanner_->nextToken();
			return std::make_shared<const NumberFactor>(number);
		}
		else if (token_->getType() == TokenType::const_true || token_->getType() == TokenType::const_false) {
			const bool boolean = (token_->getType() == TokenType::const_true) ? true : false;
			token_ = scanner_->nextToken();
			return std::make_shared<const BooleanFactor>(boolean);
		}
		else if (token_->getType() == TokenType::lparen) {
			token_ = scanner_->nextToken();
			auto _expression = expression();
			if (_expression != nullptr)
			{
				if (_expression->type == PrimitiveType::Number)
				{
					if (token_->getType() == TokenType::rparen)
					{
						token_ = scanner_->nextToken();
						return std::make_shared<const ExpressionFactor>(_expression);
					}
					else {
						logger_->error(token_->getPosition(), "- SYNTAX ERROR; \")\" is missing.");
					}
				}
				else {
					logger_->error(token_->getPosition(), "- SEMANTIC ERROR; The expression should be number");
				}
			}
			else {
				logger_->error(token_->getPosition(), "- SEMANTIC ERROR; No valid expression");
			}
		}
		else if (token_->getType() == TokenType::op_not) {
			token_ = scanner_->nextToken();
			auto _factor = factor();
			if (_factor != nullptr)
			{
				if (_factor->type == PrimitiveType::Boolean)
				{
					return std::make_shared<const NotFactor>(_factor);
				}
				else {
					logger_->error(token_->getPosition(), "- SEMANTIC ERROR; Factor is not boolean type");
				}
			}
			else {
				logger_->error(token_->getPosition(), "- SEMANTIC ERROR; No valid factor");
			}
		}
		else {
			logger_->error(token_->getPosition(), "- SYNTAX ERROR; Factor is not valid.");
		}
	}
	return nullptr;
}

std::shared_ptr<const Type> Parser::type() {
	// ident | ArrayType | RecordType
	std::string name = ident();
	if (!name.empty())
	{
		token_ = scanner_->nextToken();
		PrimitiveType primitiveType;
		if (name == "INTEGER" || name == "LONGINT")
		{
			primitiveType = PrimitiveType::Number;
		}
		else if (name == "CHAR") {
			primitiveType = PrimitiveType::String;
		}
		else if (name == "BOOLEAN") {
			primitiveType = PrimitiveType::Boolean;
		}
		else {
			auto typeNode = symbolTable_.lookup(name);
			if (typeNode != nullptr && typeNode->nodeType_ == NodeType::type_reference)
			{
				auto _type = static_cast<Type*>(const_cast<Node*>(typeNode.get()));
				return std::shared_ptr<Type>(_type);
			}
			else {
				logger_->error(token_->getPosition(), "- SEMANTIC ERROR; No defined type by identifier \"" + name + "\"");
				return nullptr;
			}
		}
		return std::make_shared<Type>(primitiveType);
	}
	else if (token_->getType() == TokenType::kw_array) {
		return array_type();
	}
	else if (token_->getType() == TokenType::kw_record) {
		return record_type();
	}
	logger_->error(token_->getPosition(), "- SYNTAX ERROR; type is not valid.");
	return nullptr;
}

std::shared_ptr<const ArrayType> Parser::array_type() {
	// "ARRAY" expression "OF" type.
	token_ = scanner_->nextToken();
	auto _expression = expression();
	if (_expression != nullptr)
	{
		if (_expression->type == PrimitiveType::Number)
		{
			if (token_->getType() == TokenType::kw_of)
			{
				token_ = scanner_->nextToken();
				auto _type = type();
				if (_type != nullptr)
				{
					return std::make_shared<const ArrayType>(_expression, _type);
				}
				else {
					logger_->error(token_->getPosition(), "- SEMANTIC ERROR; No valid type");
				}
			}
			else {
				logger_->error(token_->getPosition(), "- SYNTAX ERROR; \"OF\" keyword is missing.");
			}
		}
		else {
			logger_->error(token_->getPosition(), "- SEMANTIC ERROR; Invalid array dimension");
		}
	}
	else {
		logger_->error(token_->getPosition(), "- SEMANTIC ERROR; No valid expression");
	}
	return nullptr;
}

std::shared_ptr<const RecordType> Parser::record_type() {
	// "RECORD" FieldList {";" FieldList} "END"
	token_ = scanner_->nextToken();
	std::vector<std::shared_ptr<const Variable>> fieldList = field_list();
	if (fieldList.size() != 0)
	{
		auto record = std::make_shared<RecordType>();
		for (auto& field : fieldList) {
			record->fieldListNodes.emplace_back(field);
		}
		fieldList.clear();
		bool shouldRepeat = true;
		while (shouldRepeat)
		{
			if (token_->getType() == TokenType::semicolon)
			{
				token_ = scanner_->nextToken();
				fieldList = field_list();
				for (auto& field : fieldList) {
					for (auto& recordField : record->fieldListNodes) {
						if (field->identifier == recordField->identifier)
						{
							logger_->error(token_->getPosition(), "- SEMANTIC ERROR; " + field->identifier + " has been used in the record scope");
							shouldRepeat = false;
						}
						else {
							record->fieldListNodes.emplace_back(field);
						}
					}
				}
				if (shouldRepeat == false)
				{
					return nullptr;
				}
			}
			else {
				shouldRepeat = false;
			}
		}
		if (token_->getType() == TokenType::kw_end)
		{
			token_ = scanner_->nextToken();
			return record;
		}
		else {
			logger_->error(token_->getPosition(), "- SYNTAX ERROR; \"END\" keyword is missing.");
		}
	}
	else {
		logger_->error(token_->getPosition(), "- SEMANTIC ERROR; A record has no valid or unique identifiers.");
	}
	return nullptr;
}

const std::vector<std::shared_ptr<const Variable>> Parser::field_list() {
	// [IdentList ":" type] -> optional
	std::vector<std::shared_ptr<const Variable>> fieldList;
	const std::vector<std::string> identList = ident_list();
	if (identList.size() != 0)
	{
		if (token_->getType() == TokenType::colon)
		{
			token_ = scanner_->nextToken();
			auto _type = type();
			if (_type != nullptr)
			{
				for (auto& identifier : identList) {
					fieldList.emplace_back(std::make_shared<const Variable>(identifier, _type));
				}
			}
			else {
				logger_->error(token_->getPosition(), "- SEMANTIC ERROR; No valid type");
			}
		}
		else {
			logger_->error(token_->getPosition(), "- SYNTAX ERROR; \":\" is missing");
		}
	}
	return fieldList;
}

const std::vector<std::string> Parser::ident_list() {
	// ident {"," ident}
	std::vector<std::string> identList;
	bool shouldRepeat = true;
	while (shouldRepeat)
	{
		std::string name = ident();
		if (!name.empty())
		{
			bool isNameOk = true;
			for (auto identifier : identList) {
				if (name == identifier)
				{
					isNameOk = false;
					logger_->error(token_->getPosition(), "- SEMANTIC ERROR; Identifier \"" + name + "\" has been used");
				}
			}
			if (isNameOk)
			{
				identList.emplace_back(name);
			}
			token_ = scanner_->nextToken();
			if (token_->getType() != TokenType::comma)
			{
				shouldRepeat = false;
			}
			else {
				token_ = scanner_->nextToken();
			}
		}
		else {
			shouldRepeat = false;
			//logger_->error(token_->getPosition(), "- SYNTAX ERROR; Identifier is not valid.");
		}
	}
	return identList;
}

std::shared_ptr<const ProcedureHead> Parser::procedure_heading() {
	// "PROCEDURE" ident [FormalParameters]
	token_ = scanner_->nextToken();
	std::string identifier = ident();
	if (!identifier.empty())
	{
		token_ = scanner_->nextToken();
		auto procedureHead = std::make_shared<ProcedureHead>(identifier);
		for (auto _variable : formal_parameters()) {
			procedureHead->parameters.emplace_back(_variable);
		}
		return procedureHead;
	}
	else {
		logger_->error(token_->getPosition(), "- SYNTAX ERROR; Procedure name is not valid.");
	}
	return nullptr;
}

std::shared_ptr<const ProcedureBody> Parser::procedure_body(const std::string _procedureIdentifier) {
	// declarations ["BEGIN" StatementSequence] "END" ident

	auto body = std::make_shared<ProcedureBody>();
	const std::vector<std::shared_ptr<const Variable>> variableDeclarations = declarations();
	for (auto declaration : variableDeclarations) {
		bool isDeclarationOk = true;
		for (auto bodyDeclarations : body->declarations) {
			if (declaration->identifier == bodyDeclarations->identifier) {
				isDeclarationOk = false;
				logger_->error(token_->getPosition(), "- SEMANTIC ERROR; Identifier \"" + declaration->identifier + "\" has been used");
			}
		}
		if (isDeclarationOk)
		{
			body->declarations.emplace_back(declaration);
		}
	}
	if (token_->getType() == TokenType::kw_begin)
	{
		token_ = scanner_->nextToken();
		const std::vector<std::shared_ptr<const Statement>> statementSequence = statement_sequence();
		body->statements = statementSequence;
	}
	if (token_->getType() == TokenType::kw_end)
	{
		token_ = scanner_->nextToken();
		std::string name = ident();
		if (!name.empty())
		{
			token_ = scanner_->nextToken();
			if (name == _procedureIdentifier)
			{
				return body;
			}
			else {
				logger_->error(token_->getPosition(), "- SEMANTIC ERROR; Procedure name is not same.");
			}
		}
		else {
			logger_->error(token_->getPosition(), "- SYNTAX ERROR; Procedure name is not identified.");
		}
	}
	else {
		logger_->error(token_->getPosition(), "- SYNTAX ERROR; \"END\" keyword is missing.");
	}
	return nullptr;
}

const std::vector<std::shared_ptr<const Variable>> Parser::formal_parameters() {
	// "(" [FPSection {";" FPSection} ] ")"
	std::vector<std::shared_ptr<const Variable>> formalParameters;
	if (token_->getType() == TokenType::lparen)
	{
		token_ = scanner_->nextToken();
		formalParameters = fp_section();
		if (formalParameters.size() > 0)
		{
			bool shouldRepeat = true;
			while (shouldRepeat)
			{
				if (token_->getType() == TokenType::semicolon)
				{
					token_ = scanner_->nextToken();
					for (auto p : fp_section()) {
						bool isParameterOk = true;
						for (auto parameter : formalParameters) {
							if (p->identifier == parameter->identifier)
							{
								logger_->error(token_->getPosition(), "- SEMANTIC ERROR; Parameter identifier \"" + p->identifier + "\" has been used");
							}
						}
						if (isParameterOk)
						{
							formalParameters.emplace_back(p);
						}
					}
				}
				else {
					shouldRepeat = false;
				}
			}
		}
		if (token_->getType() == TokenType::rparen)
		{
			token_ = scanner_->nextToken();
		}
		else {
			logger_->error(token_->getPosition(), "- SYNTAX ERROR; \")\" is missing.");
		}
	}
	else {
		logger_->error(token_->getPosition(), "- SYNTAX ERROR; \"(\" is missing.");
	}
	return formalParameters;
}

const std::vector<std::shared_ptr<const Variable>> Parser::fp_section() {
	// ["VAR"]	IdentList ":" type
	std::vector<std::shared_ptr<const Variable>> parameters;
	bool hasVarKeyword = false;
	if (token_->getType() == TokenType::kw_var)
	{
		hasVarKeyword = true;
		token_ = scanner_->nextToken();
	}
	const std::vector<std::string> identList = ident_list();
	if (identList.size() != 0)
	{
		if (token_->getType() == TokenType::colon)
		{
			token_ = scanner_->nextToken();
			auto _type = type();
			if (_type != nullptr)
			{
				if (hasVarKeyword && (_type->primitiveType == PrimitiveType::Array || _type->primitiveType == PrimitiveType::Record))
				{
					logger_->error(token_->getPosition(), "- SEMANTIC ERROR; \"VAR\" keyword can not be used with structured types.");
				}
				else {
					for (auto identifier : identList) {
						parameters.emplace_back(std::make_shared<const Variable>(identifier, _type));
					}
				}
			}
			else {
				logger_->error(token_->getPosition(), "- SEMANTIC ERROR; No valid type.");
			}
		}
		else {
			logger_->error(token_->getPosition(), "- SYNTAX ERROR; \":\" is missing.");
		}
	}
	return parameters;
}

const std::vector<std::shared_ptr<const Statement>> Parser::statement_sequence() {
	// statement {";" statement}
	std::vector<std::shared_ptr<const Statement>> statementList;
	auto s = statement();
	if (s != nullptr)
	{
		statementList.emplace_back(s);
		bool shouldProceed = true;
		while (shouldProceed)
		{
			if (token_->getType() == TokenType::semicolon)
			{
				token_ = scanner_->nextToken();
				s.reset();
				s = statement();
				if (s == nullptr)
				{
					shouldProceed = false;
				}
				else {
					statementList.emplace_back(s);
				}
			}
			else {
				shouldProceed = false;
			}
		}
	}
	return statementList;
}

std::shared_ptr<const Statement> Parser::statement() {
	// [assignment | ProcedureCall | IfStatement | WhileStatement]
	if (token_->getType() == TokenType::kw_if)
	{
		return if_statement();
	}
	else if (token_->getType() == TokenType::kw_while) {
		return while_statement();
	}
	else {
		std::string identifier = ident();
		if (!identifier.empty())
		{
			token_ = scanner_->nextToken();
			auto variable = symbolTable_.lookup(identifier);
			if (variable != nullptr)
			{
				if (variable->nodeType_ == NodeType::constant_reference || variable->nodeType_ == NodeType::type_reference)
				{
					logger_->error(token_->getPosition(), "- SEMANTIC ERROR; Constant or Type declarations can not be changed");
				}
				else if (variable->nodeType_ == NodeType::variable_reference) {
					auto _var = static_cast<VarVariable*>(const_cast<Node*>(variable.get()));
					auto _selector = selector();
					if (_selector != nullptr)
					{
						if (_var->primitiveType == _selector->type)
						{
							_var->selector = _selector;
						}
						else {
							logger_->error(token_->getPosition(), "- SEMANTIC ERROR; Selector type is not convenient with the variable type");
							return nullptr;
						}
					}
					auto _assignment = assignment();
					if (_assignment != nullptr)
					{
						if (_var->primitiveType == _assignment->expression->type) {
							return _assignment;
						}
						else {
							logger_->error(token_->getPosition(), "- SEMANTIC ERROR; Variable and expression type do not match");
						}
					}
					else {
						logger_->error(token_->getPosition(), "- SEMANTIC ERROR; No valid assignment");
					}
				}
				else if (variable->nodeType_ == NodeType::procedure) {
					auto _procedureCall = procedure_call();
					if (_procedureCall != nullptr)
					{
						auto _procedure = static_cast<ProcedureVariable*>(const_cast<Node*>(variable.get()));
						if (_procedureCall->actualParameters.size() == _procedure->parameters.size())
						{
							bool isParameterTypesOk = true;
							for (int i = 0; i < _procedure->parameters.size(); i++) {
								if (_procedureCall->actualParameters[i]->type != _procedure->parameters[i]->primitiveType)
								{
									isParameterTypesOk = false;
									break;
								}
							}
							if (isParameterTypesOk)
							{
								return _procedureCall;
							}
							else {
								logger_->error(token_->getPosition(), "- SEMANTIC ERROR; Actual parameters types are not equal corresponding formal parameter type");
							}
						}
						else {
							logger_->error(token_->getPosition(), "- SEMANTIC ERROR; Actual parameters number is not equal formal parameters number of procedure");
						}
					}
					else {
						logger_->error(token_->getPosition(), "- SEMANTIC ERROR; No valid procedure calling");
					}
				}
			}
			else {
				logger_->error(token_->getPosition(), "- SEMANTIC ERROR; No valid identifier");
			}
		}
	}
	return nullptr;
}

std::shared_ptr<const AssignmentStatement> Parser::assignment() {
	// ident selector ":=" expression
	if (token_->getType() == TokenType::op_becomes)
	{
		token_ = scanner_->nextToken();
		auto _expression = expression();
		if (_expression != nullptr)
		{
			return std::make_shared<AssignmentStatement>(_expression);
		}
		else {
			logger_->error(token_->getPosition(), "- SEMANTIC ERROR; No valid expression after \":=\"");
		}
	}
	else {
		logger_->error(token_->getPosition(), "- SYNTAX ERROR; \":=\" is missing.");
	}
	return nullptr;
}

std::shared_ptr<const ProcedureCallStatement> Parser::procedure_call() {
	// ident selector [ActualParameters]
	auto procedureCall = std::make_shared<ProcedureCallStatement>();
	procedureCall->actualParameters = actual_parameters();
	return procedureCall;
}

const std::vector<std::shared_ptr<const Expression>> Parser::actual_parameters() {
	// "(" [expression {"," expression}] ")"
	std::vector<std::shared_ptr<const Expression>> actualParameters;
	if (token_->getType() == TokenType::lparen)
	{
		token_ = scanner_->nextToken();
		auto _expression = expression();
		if (_expression != nullptr)
		{
			actualParameters.emplace_back(_expression);
			bool shouldRepeat = true;
			while (shouldRepeat)
			{
				if (token_->getType() == TokenType::comma)
				{
					token_ = scanner_->nextToken();
					_expression.reset();
					_expression = expression();
					if (_expression != nullptr)
					{
						actualParameters.emplace_back(_expression);
					}
					else {
						shouldRepeat = false;
						logger_->error(token_->getPosition(), "- SEMANTIC ERROR; No valid expression after \",\"");
					}
				}
				else {
					shouldRepeat = false;
				}
			}
		}
		if (token_->getType() == TokenType::rparen)
		{
			token_ = scanner_->nextToken();
		}
		else {
			logger_->error(token_->getPosition(), "- SYNTAX ERROR; \")\" is missing");
		}
	}
	else {
		logger_->error(token_->getPosition(), "- SYNTAX ERROR; \"(\" is missing");
	}
	return actualParameters;
}

std::shared_ptr<const IfStatement> Parser::if_statement() {
	// "IF" expression "THEN" StatementSequence {"ELSIF" expression "THEN" StatementSequence} ["ELSE" StatementSequence] "END"
	token_ = scanner_->nextToken();
	auto _expression = expression();
	if (_expression != nullptr)
	{
		if (_expression->type != PrimitiveType::Boolean)
		{
			if (token_->getType() == TokenType::kw_then)
			{
				auto ifStatement = std::make_shared<IfStatement>(_expression);
				token_ = scanner_->nextToken();
				const std::vector<std::shared_ptr<const Statement>> statementList = statement_sequence();
				for (auto _statement : statementList) {
					ifStatement->statements.emplace_back(_statement);
				}
				if (token_->getType() == TokenType::kw_elsif)
				{
					bool shouldProceed = true;
					while (shouldProceed)
					{
						token_ = scanner_->nextToken();
						auto _innerExpression = expression();
						if (_expression != nullptr)
						{
							if (_expression->type != PrimitiveType::Boolean)
							{
								if (token_->getType() == TokenType::kw_then) {
									auto elseIfStatement = std::make_shared<ElseIf>(_innerExpression);
									token_ = scanner_->nextToken();
									const std::vector<std::shared_ptr<const Statement>> innerStatementList = statement_sequence();
									for (auto _innerStatement : statementList) {
										elseIfStatement->statements.emplace_back(_innerStatement);
									}
									ifStatement->elseIfNodes.emplace_back(elseIfStatement);
									if (token_->getType() != TokenType::kw_elsif)
									{
										shouldProceed = false;
									}
								}
								else {
									logger_->error(token_->getPosition(), "- SYNTAX ERROR; \"THEN\" keyword is missing.");
									shouldProceed = false;
									return nullptr;
								}
							}
							else {
								logger_->error(token_->getPosition(), "- SEMANTIC ERROR; Expression is not boolean type");
								return nullptr;
							}
						}
						else {
							logger_->error(token_->getPosition(), "- SEMANTIC ERROR; No valid expression in else if block");
							return nullptr;
						}

					}
				}
				if (token_->getType() == TokenType::kw_else)
				{
					token_ = scanner_->nextToken();
					auto elseStatement = std::make_shared<Else>();
					const std::vector<std::shared_ptr<const Statement>> innerStatementList = statement_sequence();
					for (auto _innerStatement : statementList) {
						elseStatement->statements.emplace_back(_innerStatement);
					}
					ifStatement->elseNode = elseStatement;
				}
				if (token_->getType() == TokenType::kw_end)
				{
					token_ = scanner_->nextToken();
					return ifStatement;
				}
				else {
					logger_->error(token_->getPosition(), "- SYNTAX ERROR; \"END\" keyword is missing.");
				}
			}
			else {
				logger_->error(token_->getPosition(), "- SYNTAX ERROR; \"THEN\" keyword is missing.");
			}
		}
		else {
			logger_->error(token_->getPosition(), "- SEMANTIC ERROR; Expression is not boolean");
		}
	}
	else {
		logger_->error(token_->getPosition(), "- SEMANTIC ERROR; No valid expression");
	}
	return nullptr;
}

std::shared_ptr<const WhileStatement> Parser::while_statement() {
	// "WHILE" expression "DO" StatementSequence "END"
	token_ = scanner_->nextToken();
	auto _expression = expression();
	if (_expression != nullptr)
	{
		if (_expression->type == PrimitiveType::Boolean)
		{
			if (token_->getType() == TokenType::kw_do)
			{
				auto whileStatement = std::make_shared<WhileStatement>(_expression);
				token_ = scanner_->nextToken();
				const std::vector<std::shared_ptr<const Statement>> statementList = statement_sequence();
				for (auto statement : statementList) {
					whileStatement->statements.emplace_back(statement);
				}
				if (token_->getType() == TokenType::kw_end)
				{
					token_ = scanner_->nextToken();
					return whileStatement;
				}
				else {
					logger_->error(token_->getPosition(), "- SYNTAX ERROR; \"END\" is missing");
				}
			}
			else {
				logger_->error(token_->getPosition(), "- SYNTAX ERROR; \"DO\" keyword is missing");
			}
		}
		else {
			logger_->error(token_->getPosition(), "- SEMANTIC ERROR; Expression is not boolean");
		}
	}
	else {
		logger_->error(token_->getPosition(), "- SEMANTIC ERROR; No valid expression");
	}
	return nullptr;
}

const std::shared_ptr<const Selector> Parser::selector() {
	// {"." ident | "[" expression "]"} -> repetition
	bool shouldRepeat = true;
	std::shared_ptr<Selector> selector = nullptr;
	int selectorIndex = -1;
	while (shouldRepeat)
	{
		selectorIndex++;
		if (token_->getType() == TokenType::period)	// RECORD
		{
			token_ = scanner_->nextToken();
			std::string identifier = ident();
			if (!identifier.empty())
			{
				auto variable = std::make_shared<const Variable>(identifier);
				auto recordSelector = std::make_shared<RecordSelector>(variable);
				if (selectorIndex == 0)
				{
					selector = recordSelector;
				}
				else if (selectorIndex > 0) {
					selector->innerSelectors.insert({ selectorIndex, recordSelector });
				}
				token_ = scanner_->nextToken();
				if (token_->getType() != TokenType::period || token_->getType() != TokenType::lbrack)
				{
					shouldRepeat = false;
				}
			}
			else {
				logger_->error(token_->getPosition(), "- SYNTAX ERROR; Selector is not valid.");
				shouldRepeat = false;
			}
		}
		else if (token_->getType() == TokenType::lbrack) {	// ARRAY
			token_ = scanner_->nextToken();
			auto _expression = expression();
			if (_expression != nullptr)
			{
				if (_expression->type == PrimitiveType::Number)
				{
					if (token_->getType() == TokenType::rbrack)
					{
						token_ = scanner_->nextToken();
						auto arraySelector = std::make_shared<ArraySelector>(_expression);
						if (selectorIndex == 0)
						{
							selector = arraySelector;
						}
						else if (selectorIndex > 0) {
							selector->innerSelectors.insert({ selectorIndex, arraySelector });
						}
						if (token_->getType() != TokenType::period || token_->getType() != TokenType::lbrack)
						{
							shouldRepeat = false;
						}
					}
					else {
						shouldRepeat = false;
						logger_->error(token_->getPosition(), "- SYNTAX ERROR; \"]\" is missing");
					}
				}
				else {
					logger_->error(token_->getPosition(), "- SEMANTIC ERROR; Array index must be number.");
				}
			}
			else {
				shouldRepeat = false;
				logger_->error(token_->getPosition(), "- SEMANTIC ERROR; No valid expression.");
			}
		}
		else {
			shouldRepeat = false;
		}
	}
	return selector;
}