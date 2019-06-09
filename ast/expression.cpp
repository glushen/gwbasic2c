#include <utility>
#include <memory>
#include <cassert>
#include "node.h"
#include "expression.h"
#include "util.h"

ast::Expression::Expression(gw::Type type):
        type(type) { }

ast::ConstExpression::ConstExpression(gw::Type type, std::string&& valueToPrint):
        Expression(type),
        valueToPrint(valueToPrint) { }

void ast::ConstExpression::provideInfo(ast::ProgramInfo& programInfo) const {
    // nothing to provide
}

void ast::ConstExpression::print(std::ostream& stream) const {
    stream << valueToPrint;
}

ast::IntConstExpression::IntConstExpression(short value):
        ConstExpression(gw::INT, util::to_string("%d", value)) { }

ast::FloatConstExpression::FloatConstExpression(float value):
        ConstExpression(gw::FLOAT, util::to_string("%.7g", value)) { }

ast::DoubleConstExpression::DoubleConstExpression(double value):
        ConstExpression(gw::DOUBLE, util::to_string("%.16g", value)) { }

ast::StringConstExpression::StringConstExpression(const std::string& value):
        ConstExpression(gw::STRING, '"' + util::escape(value) + '"') { }

ast::VariableExpression::VariableExpression(std::string name, gw::Type type):
        Expression(type),
        name(std::move(name)) {
    assert(isReference(type));
}

std::string ast::VariableExpression::getPrintableType() const {
    switch (type) {
        case gw::INT_REF: return "gw_int";
        case gw::FLOAT_REF: return "float";
        case gw::DOUBLE_REF: return "double";
        case gw::STRING_REF: return "std::string";
        default: assert(false);
    }
}

std::string ast::VariableExpression::getPrintableName() const {
    switch (type) {
        case gw::INT_REF: return "_" + name + "_i";
        case gw::FLOAT_REF: return "_" + name + "_f";
        case gw::DOUBLE_REF: return "_" + name + "_d";
        case gw::STRING_REF: return "_" + name + "_s";
        default: assert(false);
    }
}

std::string ast::VariableExpression::getPrintableDefaultValue() const {
    return type == gw::STRING_REF ? "\"\"" : "0";
}

void ast::VariableExpression::provideInfo(ast::ProgramInfo& programInfo) const {
    programInfo.variableDefinitions.insert(getPrintableType() + " " + getPrintableName() + " = " + getPrintableDefaultValue());
}

void ast::VariableExpression::print(std::ostream& stream) const {
    stream << getPrintableName();
}

ast::VectorDimExpression::VectorDimExpression(VariableExpression variable, std::vector<std::unique_ptr<Expression>> new_sizes):
        Expression(gw::VOID),
        variable(std::move(variable)),
        new_sizes(castOrThrow(std::move(new_sizes), gw::INT)) { }

void ast::VectorDimExpression::provideInfo(ast::ProgramInfo& programInfo) const {
    programInfo.variableDefinitions.insert("std::vector<" + variable.getPrintableType() + "> " + variable.getPrintableName() + "v = { }");
    programInfo.variableDefinitions.insert("std::vector<gw_int> " + variable.getPrintableName() + "i = { }");
    programInfo.coreFiles.insert(gw::core::vector);
    for (auto& child : new_sizes) {
        child->provideInfo(programInfo);
    }
}

void ast::VectorDimExpression::print(std::ostream& stream) const {
    stream << "dim(" << variable.getPrintableName() << "v," << variable.getPrintableName() << "i,{";
    joinAndPrint(stream, new_sizes);
    stream << "})";
}

ast::VectorGetElementExpression::VectorGetElementExpression(VariableExpression variable, std::vector<std::unique_ptr<Expression>> indexes):
        Expression(variable.type),
        variable(std::move(variable)),
        indexes(castOrThrow(std::move(indexes), gw::INT)) { }

void ast::VectorGetElementExpression::provideInfo(ast::ProgramInfo& programInfo) const {
    programInfo.variableDefinitions.insert("std::vector<" + variable.getPrintableType() + "> " + variable.getPrintableName() + "v = { }");
    programInfo.variableDefinitions.insert("std::vector<gw_int> " + variable.getPrintableName() + "i = { }");
    programInfo.coreFiles.insert(gw::core::vector);
    for (auto& child : indexes) {
        child->provideInfo(programInfo);
    }
}

void ast::VectorGetElementExpression::print(std::ostream& stream) const {
    stream << "get(" << variable.getPrintableName() << "v," << variable.getPrintableName() << "i,{";
    joinAndPrint(stream, indexes);
    stream << "})";
}

ast::FunctionExpression::FunctionExpression(const gw::logic::File* logicFile, std::vector<std::unique_ptr<Expression>> argumentList):
        Expression(logicFile->returnType),
        logicFile(logicFile),
        argumentList(std::move(argumentList)) { }

void ast::FunctionExpression::provideInfo(ast::ProgramInfo& programInfo) const {
    programInfo.logicFiles.insert(logicFile);
    for (auto& child : argumentList) {
        child->provideInfo(programInfo);
    }
}

void ast::FunctionExpression::print(std::ostream& stream) const {
    stream << logicFile->name << '(';
    joinAndPrint(stream, argumentList);
    stream << ')';
}

std::unique_ptr<ast::FunctionExpression> ast::retrieveFunctionExpression(const std::string& name, std::vector<std::unique_ptr<Expression>> argumentList) {
    if (gw::logic::BY_FUNCTION_NAME.count(name) == 0) {
        throw std::invalid_argument("Function " + name + " is not found");
    }

    auto& logicFiles = gw::logic::BY_FUNCTION_NAME.at(name);
    const gw::logic::File* correctableLogicFile = nullptr;

    for (auto logicFile: logicFiles) {
        if (logicFile->argumentTypes.size() != argumentList.size()) {
            continue;
        }

        bool logicFileIsCorrect = true;
        bool logicFileIsCorrectable = true;

        for (int i = 0; i < argumentList.size(); i++) {
            gw::Type actualType = argumentList[i]->type;
            gw::Type expectedType = logicFile->argumentTypes[i];

            bool typeIsCastableImplicitly = castableImplicitly(actualType, expectedType);
            logicFileIsCorrect = logicFileIsCorrect && typeIsCastableImplicitly;

            bool typeIsCastable = typeIsCastableImplicitly || castableExplicitly(actualType, expectedType);
            logicFileIsCorrectable = logicFileIsCorrectable && typeIsCastable;
        }

        if (logicFileIsCorrect) {
            return std::make_unique<FunctionExpression>(logicFile, std::move(argumentList));
        }

        if (logicFileIsCorrectable) {
            correctableLogicFile = logicFile;
        }
    }

    if (correctableLogicFile != nullptr) {
        for (int i = 0; i < argumentList.size(); i++) {
            argumentList[i] = castOrThrow(std::move(argumentList[i]), correctableLogicFile->argumentTypes[i]);
        }

        return std::make_unique<ast::FunctionExpression>(correctableLogicFile, std::move(argumentList));
    }

    throw std::invalid_argument("Function " + name + " with required signature is not found");
}

ast::CastedExpression::CastedExpression(std::unique_ptr<Expression> expression, gw::Type type):
        Expression(type),
        expression(std::move(expression)) { }

void ast::CastedExpression::provideInfo(ast::ProgramInfo& programInfo) const {
    expression->provideInfo(programInfo);
}

void ast::CastedExpression::print(std::ostream& stream) const {
    expression->print(stream);
}

bool ast::castableImplicitly(gw::Type sourceType, gw::Type targetType) {
    if (sourceType == targetType) return true;

    switch (targetType) {
        case gw::DOUBLE:
            switch (sourceType) {
                case gw::DOUBLE_REF:
                case gw::FLOAT:
                case gw::FLOAT_REF:
                case gw::INT:
                case gw::INT_REF:
                    return true;
                default:
                    return false;
            }
        case gw::FLOAT:
            switch (sourceType) {
                case gw::FLOAT_REF:
                case gw::INT:
                case gw::INT_REF:
                    return true;
                default:
                    return false;
            }
        case gw::INT:
            return sourceType == gw::INT_REF;
        case gw::STRING:
            return sourceType == gw::STRING_REF;
        default:
            return false;
    }
}

bool ast::castableExplicitly(gw::Type sourceType, gw::Type targetType) {
    switch (targetType) {
        case gw::INT:
            switch (sourceType) {
                case gw::FLOAT:
                case gw::FLOAT_REF:
                case gw::DOUBLE:
                case gw::DOUBLE_REF:
                    return true;
                default:
                    return false;
            }
        case gw::FLOAT:
            switch (sourceType) {
                case gw::DOUBLE:
                case gw::DOUBLE_REF:
                    return true;
                default:
                    return false;
            }
        default:
            return false;
    }
}

std::unique_ptr<ast::Expression> ast::castOrThrow(std::unique_ptr<ast::Expression> expression, gw::Type targetType) {
    if (castableImplicitly(expression->type, targetType)) {
        return std::make_unique<CastedExpression>(std::move(expression), targetType);
    } else if (castableExplicitly(expression->type, targetType)) {
        std::string functionName;
        switch (targetType) {
            case gw::INT:
                functionName = "cint";
                break;
            case gw::FLOAT:
                functionName = "csng";
                break;
            default:
                assert(false);
                break;
        }
        std::vector<std::unique_ptr<Expression>> argumentList;
        argumentList.push_back(std::move(expression));
        return std::move(retrieveFunctionExpression(functionName, std::move(argumentList)));
    } else {
        throw std::invalid_argument("Cannot cast " + to_string(expression->type) + " to " + to_string(targetType));
    }
}

std::vector<std::unique_ptr<ast::Expression>> ast::castOrThrow(std::vector<std::unique_ptr<Expression>> expressions, gw::Type targetType) {
    for (auto& expression : expressions) {
        expression = castOrThrow(std::move(expression), targetType);
    }
    return expressions;
}

std::unique_ptr<ast::Expression> ast::convertToString(std::unique_ptr<ast::Expression> expression) {
    if (expression->type == gw::STRING || expression->type == gw::STRING_REF) {
        return std::make_unique<CastedExpression>(std::move(expression), gw::STRING);
    } else {
        std::vector<std::unique_ptr<Expression>> arguments;
        arguments.push_back(move(expression));
        return ast::retrieveFunctionExpression("str$", move(arguments));
    }
}

ast::PrintExpression::PrintExpression():
        Expression(gw::VOID),
        newLineExpression("\n") { }

void ast::PrintExpression::provideInfo(ast::ProgramInfo& programInfo) const {
    for (auto& expression : expressions) {
        expression->provideInfo(programInfo);
    }
    programInfo.coreFiles.insert(gw::core::print);
    if (printNewLine) {
        newLineExpression.provideInfo(programInfo);
    }
}

void ast::PrintExpression::print(std::ostream& stream) const {
    stream << "print({";
    joinAndPrint(stream, expressions);

    if (printNewLine) {
        if (!expressions.empty()) {
            stream << ',';
        }
        newLineExpression.print(stream);
    }

    stream << "})";
}

void ast::PrintExpression::addExpression(std::unique_ptr<ast::Expression> expression) {
    expressions.push_back(convertToString(move(expression)));
}

ast::InputExpression::InputExpression(std::unique_ptr<Expression> prompt, std::vector<std::unique_ptr<ast::Expression>> expressions):
        Expression(gw::VOID),
        prompt(std::move(prompt)),
        expressions(std::move(expressions)) {
    for (auto& expression : expressions) {
        if (!isReference(expression->type)) {
            throw std::invalid_argument("Expected reference, found " + to_string(expression->type));
        }
    }
}

void ast::InputExpression::provideInfo(ast::ProgramInfo& programInfo) const {
    prompt->provideInfo(programInfo);
    for (auto& expression : expressions) {
        expression->provideInfo(programInfo);
    }
    programInfo.coreFiles.insert(gw::core::input);
}

void ast::InputExpression::print(std::ostream& stream) const {
    stream << "input(";
    prompt->print(stream);
    stream << ",{";
    joinAndPrint(stream, expressions);
    stream << "})";
}
