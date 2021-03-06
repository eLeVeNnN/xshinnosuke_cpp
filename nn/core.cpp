#include "core.h"
#include "../utils/toolkit.h"


namespace GlobalGraph {
	// global parameters
	Variable* INPUTS = NULL;
	Variable* OUTPUTS = NULL;
	vector<Variable*> GRAPH;
	bool IS_TRAINING = true;
};

template <class T>
vector<T*> GlobalGraph::topological_sort(T* inputs, T* outputs) {
	vector<T*> sorted_graph;
	vector<T*> outs{ outputs };
	vector<T*> ins{ inputs };

	unordered_map<string, int> name_dict;
	unordered_map<T*, unordered_map<string, unordered_set<T*>> > G;
	while (ins.size() > 0)
	{
		T* n = ins.back();
		ins.pop_back();
		if (G.count(n) == 0) {
			unordered_map<string, unordered_set<T*>>
				m1{ {"in", unordered_set<T*>()},
					{"out", unordered_set<T*>()} };
			G[n] = m1;
		}
		for (auto m = n->out_bounds.begin(); m != n->out_bounds.end(); ++m) {
			if (G.count(*m) == 0) {
				unordered_map<string, unordered_set<T*>>
					m1{ {"in", unordered_set<T*>()},
						{"out", unordered_set<T*>()} };
				G[*m] = m1;
			}
			G[n]["out"].insert(*m);
			G[*m]["in"].insert(n);
			ins.push_back(*m);
		}
	}

	unordered_set<T*> S{ inputs };

	while (S.size() > 0)
	{
		T* n = *(--S.end());
		S.erase(--S.end());
		sorted_graph.push_back(n);

		if (n->name == "") {
			string className = n->get_className();
			for (int i = 0; i < className.size(); ++i) {
				className[i] = tolower(className[i]);
			}
			n->name = className +
				to_string(name_dict[n->get_className()]);
			name_dict[n->get_className()]++;
		}

		if (std::find(outs.begin(), outs.end(), n) != outs.end()) {
			continue;
		}
		for (auto m = n->out_bounds.begin(); m != n->out_bounds.end(); ++m) {
			G[n]["out"].erase(*m);
			G[*m]["in"].erase(n);
			if (G[*m]["in"].size() == 0) {
				S.insert(*m);
			}
		}
	}
	return sorted_graph;
}


void GlobalGraph::reset_graph() {
	for (auto node = GlobalGraph::GRAPH.rbegin(); 
		node != GlobalGraph::GRAPH.rend(); ++node) {
		if ((*node)->retain) {
			(*node)->reset();
		}
		else {
			delete (*node);
			/**node = NULL;*/
		}
	}
	GlobalGraph::INPUTS = NULL;
	GlobalGraph::OUTPUTS = NULL;
}

Variable::Variable(MatrixType* data, const vector<Variable*>& in_bounds,
	const vector<Variable*>& out_bounds, const string& name,
	bool requires_grad) {
	this->in_bounds = in_bounds;
	this->out_bounds = out_bounds;
	this->name = name;
	this->requires_grad = requires_grad;
	this->data = data;
	this->shape = Shape({ data->rows(), data->cols() });
	this->retain = false;
	this->grad_fn = NULL;
	this->data_delete_flag = false;
	this->className = "Variable";
}

//Variable::Variable(BlockType* data, const vector<Variable*>& in_bounds,
//	const vector<Variable*>& out_bounds, const string& name,
//	bool requires_grad) {
//	this->in_bounds = in_bounds;
//	this->out_bounds = out_bounds;
//	this->name = name;
//	this->requires_grad = requires_grad;
//	decltype(data) data = data;
//	this->shape = Shape({ data->rows(), data->cols() });
//	this->retain = false;
//	this->grad_fn = NULL;
//}


Variable::Variable(const Variable& v) {
	this->data = v.data;
	this->shape = v.shape;
	this->name = v.name;
	this->requires_grad = v.requires_grad;
	this->grad = v.grad;
}


Variable::~Variable() {
	if (this->data != NULL) {
		delete this->data;
		this->data = NULL;
	}
	if (this->grad != NULL) {
		delete this->grad;
		this->grad = NULL;
	}


	/*for (auto in_bound = this->in_bounds.begin(); 
		in_bound != this->in_bounds.end();
		++in_bound) {
		if (*in_bound != NULL && !((*in_bound)->retain)) {
			delete *in_bound;
			*in_bound = NULL;
		}
	}
	
	for (auto out_bound = this->out_bounds.begin(); 
		out_bound != this->out_bounds.end();
		++out_bound) {
		if (*out_bound != NULL && !((*out_bound)->retain)) {
			delete *out_bound;
			*out_bound = NULL;
		}
	}*/
}

void Variable::reset() {
	this->in_bounds.clear();
	/*for (auto out_bound = this->out_bounds.begin(); 
		out_bound != this->out_bounds.end(); ++out_bound) {
		if (*out_bound != NULL && !(*out_bound)->retain) {
			delete *out_bound;
			*out_bound = NULL;
		}
	}
	this->out_bounds.clear();*/
	vector<Variable*>().swap(this->out_bounds);
}


void Variable::requires_grad_(bool requires_grad) {
	this->requires_grad = requires_grad;
}

vector<Variable*>& Variable::get_parameters() {
	return this->parameters;
}

void Variable::set_parameters(const initializer_list<Variable*>& l) {
	for (auto v = l.begin(); v != l.end(); ++v) {
		this->parameters.push_back(*v);
	}
}

void Variable::backward(){
	if (this->grad_fn == NULL) {
		cout << "can't solve grad on " << this << endl;
		return;
	}
	if (this->data->size() == 1) {
		if (this->grad == NULL) {
			Eigen::MatrixXf* constant_one = new Eigen::MatrixXf(1, 1);
			constant_one->setOnes();
			this->grad = constant_one;
		}
		else {
			cout << "grad can be implicitly created only for scalar outputs" << endl;
			return;
		}
	}
	if (GlobalGraph::OUTPUTS == NULL)
		GlobalGraph::OUTPUTS = this;

	if (GlobalGraph::INPUTS != NULL) {
		GlobalGraph::GRAPH = GlobalGraph::topological_sort(GlobalGraph::INPUTS,
			GlobalGraph::OUTPUTS);
		GlobalGraph::INPUTS->retain_grad();
		GlobalGraph::OUTPUTS->retain_grad();
		for (auto node = GlobalGraph::GRAPH.rbegin(); 
			node != GlobalGraph::GRAPH.rend(); ++node) {
			if ((*node)->grad_fn == NULL) {
				(*node)->reset();
				continue;
			}
			(*node)->grad_fn(*node);
			if ((*node)->retain) {
				(*node)->reset();
			}
			else {
				delete *node;
			}
		}
		// GlobalGraph::reset_graph();
		GlobalGraph::INPUTS = NULL;
		GlobalGraph::OUTPUTS = NULL;
	}
}

void Variable::backward(Eigen::MatrixXf* gradients) {
	if (gradients->rows() != this->data->rows() || gradients->cols() !=
		this->data->cols()) {
		return;
	}
	this->grad = gradients;
	this->backward();
}

void Variable::retain_grad() {
	this->retain = true;
}

Eigen::MatrixXf& Variable::item() {
	return *(this->data);
}

void Variable::zero_grad() {
	int rows = this->data->rows();
	int cols = this->data->cols();
	Eigen::MatrixXf* zeros = new Eigen::MatrixXf(rows, cols);
	zeros->setZero();
	this->grad = zeros;
}


void Variable::set_grad_fn_name(const string& name) {
	this->grad_fn_name = name;
}

Shape& Variable::size() {
	return this->shape;
}

int Variable::size(int index) {
	return this->shape[index];
}

void Variable::set_block(int i, int j, int h, int w, Variable* block) {
	this->data->block(i, j, h, w) = *(block->data);
}

string& Variable::get_className() {
	return this->className;
}


// ############################### Layer

Layer::~Layer() {
	if (this->input_data != NULL && !this->input_data->data_delete_flag) {
		delete this->input_data;
	}
	this->input_data = NULL;
	if (this->data != NULL && !this->data->data_delete_flag) {
		delete this->data;
	}
	this->data = NULL;
}

string& Layer::get_className() {
	return this->className;
}

Layer* Layer::operator()(Layer* inbound) {
	this->input_shape = inbound->shape;
	this->shape = this->compute_output_shape(inbound->shape);
	this->in_bounds.push_back(inbound);
	inbound->out_bounds.push_back(this);
	return this;
}

void Layer::connect(Layer* inbound) {
	if (inbound == NULL) {
		if (!this->input_shape.initialize_flag) {
			throw "must specify input_shape";
		}
	}
	else {
		this->input_shape = inbound->shape;
	}
	this->shape = this->compute_output_shape(this->input_shape);
	if (inbound != NULL) {
		this->in_bounds.push_back(inbound);
		inbound->out_bounds.push_back(this);
	}
}

void Layer::initial_params(Shape& input_shape) {

}

void Layer::initial_params() {

}

Shape Layer::compute_output_shape(Shape& input_shape) {
	return input_shape;
}

int Layer::params_count() {
	int total_params = 0;
	for (auto v = this->variables.begin(); v != this->variables.end(); ++v) {
		total_params += (*v)->data->size();
	}
	return total_params;
}


void Layer::feed_variable_to_next_layer(Variable* data) {
	for (vector<Layer*>::iterator out_bound = this->out_bounds.begin(); 
		out_bound != this->out_bounds.end(); ++out_bound) {
		(*out_bound)->input_data = data;
	}

}

void Layer::backward() {
	this->data->grad_fn(this->data);
}
