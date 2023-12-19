#include "processor.h"

int main() {
	Processor* mProcessor = new Processor();
	mProcessor->configEnv();
	
	std::vector<const char*> picPaths = { "TestData\\BunkerPreTonemapHDRColor.0300.exr",
		"TestData\\BunkerBaseColor.0300.exr" };
	const char* savePath = "Result\\DemodulationRes.exr";

	mProcessor->clear();
	mProcessor->prepareData(picPaths);
	mProcessor->configOpenGL(DEMODULATE);
	mProcessor->execute(DEMODULATE);
	mProcessor->saveRes(savePath);

	picPaths = { "TestData\\BunkerBaseColor.0300.exr",
		"TestData\\BunkerMotionVectorAndMetallicAndRoughness.0300.exr",
		"TestData\\BunkerNoV.0300.exr",
		"TestData\\Precomputed.exr",
		"TestData\\BunkerSpecular.0300.exr" };
	savePath = "Result\\BRDFDemodulationRes.exr";


	mProcessor->clear();
	mProcessor->prepareData(picPaths);
	mProcessor->configOpenGL(BRDF_DEMODULATE);
	mProcessor->execute(BRDF_DEMODULATE);
	mProcessor->saveRes(savePath);

	return 0;
}