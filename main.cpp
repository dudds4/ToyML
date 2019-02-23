#include "graph.h"
#include "nodetypes.h"
#include "layers.h"

#include <iostream>
#include <cmath>
#include <cstring>

struct SquareLoss
{
	static float loss(float yout, float yexpected)
	{
		float x = yout - yexpected;
		return x * x;
	}

	static float derivative(float yout, float yexpected)
	{
		return 2 * (yout - yexpected);
	}
};

typedef std::vector<float> floatset;

template<template<typename T> class OptimT, typename LossT>
struct BatchOptimizer
{
	typedef OptimT<LossT> OptimizerT;

	void setGraph(Graph *g) { 
		graph = g;
		nParams = graph->paramNodes.size();
		paramDerivs.resize(nParams);
	}

	void setTrainingSet(std::vector<float> *in, float* out, unsigned n)
	{ 
		inputs = in;
		outputs = out;
		setSize = n;
	}

	void runEpochs(unsigned iterations) { for(int i = 0; i < iterations; ++i) runEpoch(); }

	float forwardPass(unsigned j)
	{
		graph->setInputs(inputs[j]);
		graph->traverse();
		return graph->getOutput(0);
	}

	void updateParamsInterface() { static_cast<OptimizerT*>(this)->updateParams(); }

	void runEpoch()
	{
		memset(paramDerivs.data(), 0, sizeof(float)*nParams);
		float overallError = 0;

		// compute summed derivative
		for(unsigned j = 0; j < setSize; ++j)
		{
			float output = forwardPass(j);

			overallError += LossT::loss(output, outputs[j]);
			float baseDeriv = LossT::derivative(output, outputs[j]);

			graph->backProp(0, baseDeriv);

			for(unsigned k = 0; k < nParams; ++k)
				paramDerivs[k] += graph->paramNodes[k]->getDerivative(0);

		}

		if(overallError > lastOverallError)
			learningRate /= 2;

		lastOverallError = overallError;

		// update params
		updateParamsInterface();
	}

	void setLearningRate(float r) { learningRate = r; } 

protected:

	Graph *graph;
	std::vector<float> *inputs; 
	float* outputs;
	unsigned setSize;

	float lastOverallError = 0;
	float learningRate = 0.2;

	std::vector<float> paramDerivs;
	unsigned nParams;
};


template<typename LossT>
struct GradientDescent : public BatchOptimizer<GradientDescent, LossT>
{
	void updateParams()
	{
		unsigned nParams = this->graph->paramNodes.size();

		for(unsigned k = 0; k < nParams; ++k)
		{
			auto pNode = this->graph->paramNodes[k];
			float w = pNode->getInput();
			pNode->setInput(w - this->learningRate*this->paramDerivs[k]);
		}		
	}
};

template<typename LossT>
void batchGradientDescent(Graph& graph, floatset* inputs, floatset expectedOutputs, unsigned iterations)
{
	unsigned nParams = graph.paramNodes.size();
	float paramUpdates[nParams];
	
	unsigned n = expectedOutputs.size();

	float learningRate = 0.1*2;
	float lastOverallError = 0;


	for(unsigned i = 0; i < iterations; ++i)
	{
		memset(paramUpdates, 0, sizeof(float)*nParams);

		float overallError = 0;

		// compute summed derivative
		for(unsigned j = 0; j < n; ++j)
		{
			graph.setInputs(inputs[j]);
			graph.traverse();

			float output = graph.getOutput(0);

			// overallError += ErrorFnc(output, expectedOutputs[j]);
			// float baseDeriv = ErrorDerivFnc(output, expectedOutputs[j]);
			overallError += LossT::loss(output, expectedOutputs[j]);
			float baseDeriv = LossT::derivative(output, expectedOutputs[j]);

			graph.backProp(0, baseDeriv);

			for(unsigned k = 0; k < nParams; ++k)
				paramUpdates[k] += graph.paramNodes[k]->getDerivative(0);

		}	

		if(overallError > lastOverallError)
			learningRate /= 2;

		lastOverallError = overallError;

		// if(i % 100 == 0)
		// 	std::cout << lastOverallError << std::endl;

		// update params
		for(unsigned k = 0; k < nParams; ++k)
		{
			auto pNode = graph.paramNodes[k];
			float w = pNode->getInput();
			pNode->setInput(w - learningRate*paramUpdates[k]);
		}
		
	}

}

int main()
{
	// specify the graph

	Graph graph;
	
	// inputs x1, x2

	NodeSet<InputNode> inputs(2);
	graph.addInputNodes(inputs.getInputs());

	Layer<SigmoidNode> firstLayer(inputs.getNodes(0, 2), 2);

	firstLayer.setWeights(0, {-0.2,0.2,0.1});
	firstLayer.setWeights(1, {0.3,-0.2,0.1});

	auto v = firstLayer.getOutputNodes();	
	// v.push_back(inputs.getNodes(3,4).at(0));

	Layer<SigmoidNode> secondLayer(v, 1);
	secondLayer.setWeights(0, {1,1,1});

	// graph.addInputNodes({firstLayer.getBiasNode(), secondLayer.getBiasNode()});

	graph.addParamNodes(firstLayer.getWeightNodes());
	graph.addParamNodes(secondLayer.getWeightNodes());

	graph.outputNodes = secondLayer.getOutputNodes();

	// optimize params to get an XOR function

	float goalOutput = 0.6;
	float learningRate = 0.01;

	floatset inputValues[4];
	inputValues[0] = {0,0,1,1};
	inputValues[1] = {1,0,1,1};
	inputValues[2] = {0,1,1,1};
	inputValues[3] = {1,1,1,1};

	floatset expectedOutputs = {0,1,1,0};

	unsigned iterations = 10000;

	GradientDescent<SquareLoss> optimizer;
	// GradientDescent optimizer;
	optimizer.setGraph(&graph);
	optimizer.setTrainingSet(inputValues, expectedOutputs.data(), expectedOutputs.size());
	optimizer.runEpochs(iterations);

	// batchGradientDescent<SquareLoss>(graph, inputValues, expectedOutputs, iterations);
	// batchGradientDescent(graph, inputValues, expectedOutputs, 10000);

	// // let's try and train this graph
	// for(int i = 0; i < 1000; ++i)
	// {
	// 	float overallError = 0;
	// 	bool shouldPrint = i % 100 == 0;

	for(int j = 0; j < 4; ++j)
	{
		graph.setInputs(inputValues[j]);

		graph.traverse();
		float output = graph.getOutput(0);

		std::cout 	<< "XOR(" 
					<< inputValues[j][0] << "," << inputValues[j][1]
					<< ") = " << output << std::endl;
	
	}
	// 		overallError += pow(output-expectedOutputs[j], 2);
			
	// 		graph.backProp(0);

	// 		auto paramUpdater = [=](float w, float deriv)
	// 		{
	// 			float sign = output > expectedOutputs[j] ? 1 : -1;
	// 			return w - sign * deriv * learningRate;			
	// 		};

	// 		graph.updateParams(paramUpdater);			
	// 	}

	// 	if(overallError > lastOverallError)
	// 		learningRate /= 2;

	// 	lastOverallError = overallError;

	// 	// std::cout << "Error: " << overallError << std::endl;
	// }

	return 0;
}