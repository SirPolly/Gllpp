#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <functional>
#include <fstream>
#include <iostream>
#include <variant>

namespace Gllpp {
	class Trampoline;
	template<typename LT, typename RT>
	class Sequence;
	template<typename LT, typename RT>
	class Disjunction;






	enum class ResultType {
		Failure,
		Success
	};

	std::ostream& operator<< (std::ostream& s, ResultType result) {
		switch (result) {
		case ResultType::Success: s << "Success"; break;
		case ResultType::Failure: s << "Failure"; break;
		}
		return s;
	}


	struct ParserSuccess {
		std::string_view trail;
	};

	struct ParserFailure {
		std::string_view trail;
		std::string description;
	};

	using ParserResult = std::variant<ParserSuccess, ParserFailure>;



	class GraphvizNode {
	public:
		GraphvizNode(Trampoline& trampoline, std::string name, std::string_view str)
			: _trampoline(trampoline)
			, _name(name)
			, _str(str) {
			if (_name.empty())
				return;

			_prevStack = _topStack;
			_topStack = this;
		}

		~GraphvizNode() {
			if (_name.empty())
				return;

			_topStack = _prevStack;
		}

		void emit() const;
		void emit_end(ResultType type) const;

		const std::string& get_name() const {
			return _name;
		}

		const std::string_view& get_str() const {
			return _str;
		}

		static GraphvizNode* get_top() {
			return _topStack;
		}

	private:
		static GraphvizNode* _topStack;
		GraphvizNode* _prevStack;

		Trampoline& _trampoline;
		std::string _name;
		std::string_view _str;
	};
	GraphvizNode* GraphvizNode::_topStack = nullptr;







	class Trampoline {
	public:
		Trampoline(std::string_view str)
			: _str(str)
			, _graph("graph.dot") {
			_graph << "digraph {\n"
				<< "    rankdir=LR;\n";
		}

		~Trampoline() {
			_graph << "}";
		}

		void add(std::function<void(Trampoline&)> f) {
			Work work;
			work.f = f;

			auto topNode = GraphvizNode::get_top();
			if (topNode != nullptr) {
				work.graphName = GraphvizNode::get_top()->get_name();
				work.graphStr = GraphvizNode::get_top()->get_str();
			}

			_work.push_back(work);
		}

		void run() {
			while (!_work.empty()) {
				auto work = _work.back();
				_work.pop_back();

				GraphvizNode ls{ *this, work.graphName, work.graphStr };

				work.f(*this);

				_graph << "\n\n\n";
			}
		}

		void startGraph(std::string name, std::string_view str) {
			const auto offset = !str.empty() ? str.data() - _str.data() : _str.size();

			const auto nodeName = std::to_string(offset) + ": " + name;
			_graph << "    \"ENTRY\" -> \"" << nodeName << "\"\n"
				<< "\n";
		}

		void continueGraph(std::string prevName, std::string_view prevStr, std::string name, std::string_view str) {
			const auto offset = !str.empty() ? str.data() - _str.data() : _str.size();
			const auto prevOffset = !prevStr.empty() ? prevStr.data() - _str.data() : _str.size();

			const auto nodeName = std::to_string(offset) + ": " + name;
			const auto prevNodeName = std::to_string(prevOffset) + ": " + prevName;
			_graph << "    \"" << prevNodeName << "\" -> \"" << nodeName << "\"\n"
				<< "\n";
		}

		void endGraph(std::string prevName, std::string_view prevStr, std::string name, std::string_view str, ResultType type) {
			const auto offset = !str.empty() ? str.data() - _str.data() : _str.size();
			const auto prevOffset = !prevStr.empty() ? prevStr.data() - _str.data() : _str.size();

			const auto nodeName = std::to_string(offset) + ": " + name + " (" + (type == ResultType::Success ? "Success" : "Failure") + ")";
			const auto prevNodeName = std::to_string(prevOffset) + ": " + prevName;
			_graph << "    \"" << prevNodeName << "\" -> \"" << nodeName << "\"\n"
				<< "    \"" << nodeName << "\" [color=" << (type == ResultType::Success ? "green" : "red") << ", penwidth=5]\n"
				<< "\n";
		}

	private:
		struct Work {
			std::function<void(Trampoline&)> f;
			std::string graphName;
			std::string_view graphStr;
		};

		std::vector<Work> _work;
		std::string_view _str;

		std::ofstream _graph;
	};


	void GraphvizNode::emit() const {
		if (_name.empty())
			return;

		if (_prevStack == nullptr) {
			_trampoline.startGraph(_name, _str);
		}
		else {
			_trampoline.continueGraph(_prevStack->_name, _prevStack->_str, _name, _str);
		}
	}

	void GraphvizNode::emit_end(ResultType type) const {
		if (_name.empty())
			return;

		_trampoline.endGraph(_prevStack->_name, _prevStack->_str, _name, _str, type);
	}



	class ParserBase {
	public:
		void set_name(std::string name) {
			_name = name;
		}

		const std::string& get_name() const {
			return _name;
		}

		void set_layout(std::string layout) {
			_layout = layout;
		}

	protected:
		std::string _name;
		std::string _layout;
	};

	template<typename T>
	class ComposableParser : public ParserBase {
	public:
		/// Match str with contained grammar. Return either a list of successes or failures.
		std::vector<ParserResult> parse(std::string str) const {
			Trampoline trampoline{ str };
			std::vector<ParserResult> successes, failures;

			_parse_impl(trampoline, {}, str, [&](Trampoline& trampoline, ResultType r, std::string_view trail) {
				const auto isSuccess = r == ResultType::Success && trail.empty();

				GraphvizNode ls{ trampoline, "END", trail };

				if (r == ResultType::Success && trail.empty()) {
					ls.emit_end(ResultType::Success);
					successes.push_back(ParserSuccess{});
				}
				else {
					ls.emit_end(ResultType::Failure);
					failures.push_back(ParserFailure{ trail, "TODO" });
				}
			});

			trampoline.run();

			return !successes.empty() ? successes : failures;
		}

		template<typename F>
		void _parse_impl(Trampoline& trampoline, std::string_view layout, std::string_view str, F f) const {
			size_t i = 0;
			for (; i < str.size(); ++i) {
				if (layout.find_first_of(str[i]) == std::string::npos)
					break;
			}

			static_cast<const T*>(this)->__parse_impl_inner(trampoline, layout, str.substr(i), f);
		}

		template<typename F>
		void _gather(F f) const {
			f(*static_cast<const T*>(this));
		}

		template<typename OtherT>
		Sequence<T, OtherT> operator+(OtherT other) const {
			return { *static_cast<const T*>(this), other };
		}

		template<typename OtherT>
		Disjunction<T, OtherT> operator|(OtherT other) const {
			return { *static_cast<const T*>(this), other };
		}
	};

	class Parser : public ComposableParser<Parser> {
	public:
		Parser()
			: _wrapper(std::make_shared<SharedWrapper>()) {
		}

		template<typename P>
		Parser(P p)
			: Parser() {
			*this = p;
		}

		template<typename P>
		Parser& operator=(P p) {
			static_assert(std::is_base_of_v<ParserBase, P>);
			_wrapper->wrapper = std::make_unique<WrapperInstance<P>>(p);
			return *this;
		}

		template<typename F>
		void __parse_impl_inner(Trampoline& trampoline, std::string_view layout, std::string_view str, F f) const {
			if (_wrapper->wrapper == nullptr) {
				f(trampoline, ResultType::Failure, str);
				return;
			}

			_wrapper->wrapper->parse(trampoline, layout, str, f);
		}

	private:
		class IWrapper {
		public:
			virtual ~IWrapper() = default;

			virtual void parse(Trampoline& trampoline, std::string_view layout, std::string_view str, std::function<void(Trampoline&, ResultType, std::string_view)> f) const = 0;
		};

		template<typename P>
		class WrapperInstance final : public IWrapper {
		public:
			WrapperInstance(P p)
				: _parser(p) {
			}

			virtual void parse(Trampoline& trampoline, std::string_view layout, std::string_view str, std::function<void(Trampoline&, ResultType, std::string_view)> f) const override {
				_parser._parse_impl(trampoline, layout, str, f);
			}

		private:
			P _parser;
		};

		struct SharedWrapper {
			std::unique_ptr<IWrapper> wrapper;
		};

		std::shared_ptr<SharedWrapper> _wrapper;
	};


	template<typename P>
	class Layout : public ComposableParser<Layout<P>> {
	public:
		Layout(P p, std::string definition)
			: _p(p)
			, _definition(definition) {
		}

		template<typename F>
		void __parse_impl_inner(Trampoline& trampoline, std::string_view layout, std::string_view str, F f) const {
			_p._parse_impl(trampoline, _definition, str, f);
		}

	private:
		P _p;
		std::string _definition;
	};


	template<typename P>
	Layout<P> SetLayout(P p, std::string definition) {
		static_assert(std::is_base_of_v<ParserBase, P>);
		return { p, definition };
	}



	class Empty : public ComposableParser<Empty> {
	public:
		template<typename F>
		void __parse_impl_inner(Trampoline& trampoline, std::string_view layout, std::string_view str, F f) const {
			GraphvizNode ls{ trampoline, "Empty", str };
			ls.emit();

			f(trampoline, ResultType::Success, str);
		}
	};




	namespace {
		template<char DELIMITER, char... TRAIL>
		struct CaptureDelimiterUtil {
			static bool equals(char c) {
				if (c == DELIMITER)
					return true;

				return CaptureDelimiterUtil<TRAIL...>::equals(c);
			}
		};

		template<char DELIMITER>
		struct CaptureDelimiterUtil<DELIMITER> {
			static bool equals(char c) {
				return c == DELIMITER;
			}
		};
	}

	template<char... DELIMITERS>
	class Capture : public ComposableParser<Capture<DELIMITERS...>> {
	public:
		template<typename F>
		void __parse_impl_inner(Trampoline& trampoline, std::string_view layout, std::string_view str, F f) const {
			size_t i = 0;
			for (; i < str.size(); ++i) {
				const auto c = str[i];
				if (CaptureDelimiterUtil<DELIMITERS...>::equals(c))
					goto delimiter_found;
			}

		delimiter_found:
			const auto value = str.substr(0, i);

			GraphvizNode ls(trampoline, "Capture '" + std::string(value) + "'", str);
			ls.emit();

			if (value.empty()) {
				f(trampoline, ResultType::Failure, {});
			}

			f(trampoline, ResultType::Success, str.substr(i));
			return;
		}
	};





	class Terminal : public ComposableParser<Terminal> {
	public:
		Terminal(std::string what)
			: _what(what) {
		}

		template<typename F>
		void __parse_impl_inner(Trampoline& trampoline, std::string_view layout, std::string_view str, F f) const {
			GraphvizNode ls{ trampoline, "'" + _what + "'", str };
			ls.emit();

			if (str.size() < _what.size() || memcmp(str.data(), _what.data(), _what.size())) {
				f(trampoline, ResultType::Failure, str);
				return;
			}

			f(trampoline, ResultType::Success, str.substr(_what.size()));
		}

	private:
		std::string _what;
	};





	template<typename LT, typename RT>
	class Sequence : public ComposableParser<Sequence<LT, RT>> {
	public:
		Sequence(LT lhs, RT rhs)
			: _lhs(lhs)
			, _rhs(rhs) {
			static_assert(std::is_base_of_v<ParserBase, LT>);
			static_assert(std::is_base_of_v<ParserBase, RT>);
		}

		template<typename F>
		void __parse_impl_inner(Trampoline& trampoline, std::string_view layout, std::string_view str, F f) const {
			GraphvizNode ls(trampoline, _name, str);

			_lhs._parse_impl(trampoline, layout, str, [layout, f, ls, this](Trampoline& trampoline, ResultType result, std::string_view trail) {
				if (result == ResultType::Failure) {
					ls.emit();

					f(trampoline, ResultType::Failure, trail);
					return;
				}

				_rhs._parse_impl(trampoline, layout, trail, [=](Trampoline& trampoline, ResultType result, std::string_view trail) {
					ls.emit();

					f(trampoline, result, trail);
				});
			});
		}

	private:
		LT _lhs;
		RT _rhs;
	};




	template<typename LT, typename RT>
	class Disjunction : public ComposableParser<Disjunction<LT, RT>> {
	public:
		Disjunction(LT lhs, RT rhs)
			: _lhs(lhs)
			, _rhs(rhs) {
			static_assert(std::is_base_of_v<ParserBase, LT>);
			static_assert(std::is_base_of_v<ParserBase, RT>);
		}

		template<typename F>
		void __parse_impl_inner(Trampoline& trampoline, std::string_view layout, std::string_view str, F f) const {
			GraphvizNode ls{ trampoline, _name, str };

			_gather([&](auto& parser) {
				trampoline.add([str, layout, f, ls, &parser](Trampoline& trampoline) {
					parser._parse_impl(trampoline, layout, str, [f, ls](Trampoline& trampoline, ResultType result, std::string_view trail) {
						ls.emit();
						f(trampoline, result, trail);
					});
				});
			});
		}

		template<typename F>
		void _gather(F f) const {
			_lhs._gather(f);
			_rhs._gather(f);
		}

	private:
		LT _lhs;
		RT _rhs;
	};





	Terminal operator ""_t(const char* str, size_t size) {
		return std::string{ str, size };
	}

	template<typename P>
	Disjunction<P, Empty> Optional(P p) {
		static_assert(std::is_base_of_v<ParserBase, P>);
		return { p, Empty() };
	}

}